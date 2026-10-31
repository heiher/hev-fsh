/*
 ============================================================================
 Name        : hev-fsh-client-term-accept.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client term accept
 ============================================================================
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <hev-sys.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pwd.h>
#include <sys/ioctl.h>
#if defined(__linux__)
#include <pty.h>
#elif defined(__APPLE__) || (__MACH__)
#include <util.h>
#else
#include <termios.h>
#include <libutil.h>
#endif
#endif

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-call.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-bridge.h"
#include "hev-logger.h"
#include "hev-task-io-us.h"
#include "hev-fsh-protocol.h"

#include "hev-fsh-client-term-accept.h"

#ifdef _WIN32

typedef struct _HevPtyProc HevPtyProc;

struct _HevPtyProc
{
    HANDLE pty;
    HANDLE proc;
};

/* Close the pseudo console once the shell exits, this ends the output too. */
static DWORD WINAPI
hev_pty_proc_waiter (LPVOID data)
{
    HevPtyProc *pp = data;

    WaitForSingleObject (pp->proc, INFINITE);
    ClosePseudoConsole (pp->pty);
    CloseHandle (pp->proc);
    free (pp);

    return 0;
}

static int
hev_pty_proc_start (HevPtyProc *pp, HevFshConfig *config,
                    HevFshMessageTermInfo *term_info, HANDLE *in, HANDLE *out)
{
    STARTUPINFOEXW si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    HANDLE in_r = NULL, in_w = NULL, out_r = NULL, out_w = NULL;
    HPCON pty = NULL;
    SIZE_T size = 0;
    wchar_t cmd[MAX_PATH + 16];
    wchar_t *comspec;
    COORD coord;
    BOOL ok;

    if (hev_fsh_config_get_user (config))
        LOG_W ("fsh client term accept: user is not supported on windows");

    if (!CreatePipe (&in_r, &in_w, NULL, 0))
        return -1;
    if (!CreatePipe (&out_r, &out_w, NULL, 0))
        goto fail;

    coord.X = term_info->columns ? term_info->columns : 80;
    coord.Y = term_info->rows ? term_info->rows : 25;
    if (FAILED (CreatePseudoConsole (coord, in_r, out_w, 0, &pty)))
        goto fail;

    /* The pseudo console owns its ends now. */
    CloseHandle (in_r);
    CloseHandle (out_w);
    in_r = NULL;
    out_w = NULL;

    InitializeProcThreadAttributeList (NULL, 1, 0, &size);
    si.lpAttributeList = malloc (size);
    if (!si.lpAttributeList)
        goto fail;
    if (!InitializeProcThreadAttributeList (si.lpAttributeList, 1, 0, &size))
        goto fail_attr;
    if (!UpdateProcThreadAttribute (si.lpAttributeList, 0,
                                    PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pty,
                                    sizeof (pty), NULL, NULL))
        goto fail_attr_list;

    comspec = _wgetenv (L"COMSPEC");
    wcsncpy (cmd, comspec ? comspec : L"cmd.exe", MAX_PATH);
    cmd[MAX_PATH] = L'\0';

    si.StartupInfo.cb = sizeof (si);
    ok = CreateProcessW (NULL, cmd, NULL, NULL, FALSE,
                         EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
                         &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList (si.lpAttributeList);
    free (si.lpAttributeList);
    if (!ok)
        goto fail_pty;
    CloseHandle (pi.hThread);

    pp->pty = pty;
    pp->proc = pi.hProcess;
    *in = out_r;
    *out = in_w;

    return 0;

fail_attr_list:
    DeleteProcThreadAttributeList (si.lpAttributeList);
fail_attr:
    free (si.lpAttributeList);
fail_pty:
    ClosePseudoConsole (pty);
fail:
    if (in_r)
        CloseHandle (in_r);
    if (in_w)
        CloseHandle (in_w);
    if (out_r)
        CloseHandle (out_r);
    if (out_w)
        CloseHandle (out_w);
    return -1;
}

typedef struct _HevTaskCallPtyProc HevTaskCallPtyProc;

struct _HevTaskCallPtyProc
{
    HevTaskCall base;

    HevPtyProc *pp;
    HevFshConfig *config;
    HevFshMessageTermInfo *term_info;
    HANDLE in;
    HANDLE out;
};

/* The Win32 process creation needs more stack than a task has. */
static void
hev_pty_proc_start_entry (HevTaskCall *call)
{
    HevTaskCallPtyProc *ppc = (HevTaskCallPtyProc *)call;
    int res;

    res = hev_pty_proc_start (ppc->pp, ppc->config, ppc->term_info, &ppc->in,
                              &ppc->out);
    hev_task_call_set_retval (call, res < 0 ? NULL : call);
}

static void
hev_fsh_client_term_accept_task_entry (void *data)
{
    HevFshClientTermAccept *self = data;
    HevFshClientBase *base = data;
    HevFshMessageTermInfo mtinfo;
    HevTaskCallPtyProc *ppc;
    HevTaskCall *call;
    HevPtyProc *pp;
    HANDLE in, out, t;
    void *ptr;
    int sfd;
    int pfd;
    int res;

    res = hev_fsh_client_accept_send_accept (&self->base);
    if (res < 0)
        goto quit;

    sfd = base->fd;
    /* recv msg term info */
    res = hev_task_io_socket_recv (sfd, &mtinfo, sizeof (mtinfo), MSG_WAITALL,
                                   io_yielder, self);
    if (res != sizeof (mtinfo))
        goto quit;

    pp = calloc (1, sizeof (HevPtyProc));
    if (!pp)
        goto quit;

    call = hev_task_call_new (sizeof (HevTaskCallPtyProc), 65536);
    if (!call) {
        free (pp);
        goto quit;
    }

    ppc = (HevTaskCallPtyProc *)call;
    ppc->pp = pp;
    ppc->config = base->config;
    ppc->term_info = &mtinfo;

    ptr = hev_task_call_jump (call, hev_pty_proc_start_entry);
    in = ppc->in;
    out = ppc->out;
    hev_task_call_destroy (call);
    if (!ptr) {
        LOG_E ("%p fsh client term accept pty", self);
        free (pp);
        goto quit;
    }

    pfd = hev_bridge_new (in, out,
                          HEV_BRIDGE_OWN | HEV_BRIDGE_LF2CR | HEV_BRIDGE_DRAIN);
    if (pfd < 0) {
        CloseHandle (in);
        CloseHandle (out);
        TerminateProcess (pp->proc, 0);
        goto quit_wait;
    }

    hev_task_add_fd (hev_task_self (), pfd, POLLIN | POLLOUT);
    hev_task_io_splice (sfd, sfd, pfd, pfd, 8192, io_yielder, self);
    hev_task_io_close (pfd);

    /* Kill the shell, then its waiter releases the pseudo console. */
    TerminateProcess (pp->proc, 0);
quit_wait:
    t = CreateThread (NULL, 0, hev_pty_proc_waiter, pp, 0, NULL);
    if (t)
        CloseHandle (t);
quit:
    hev_object_unref (HEV_OBJECT (self));
}

#else

typedef struct _HevTaskCallForkPty HevTaskCallForkPty;

struct _HevTaskCallForkPty
{
    HevTaskCall base;

    int *pfd;
    HevFshConfig *config;
    HevFshMessageTermInfo *term_info;
};

static void
exec_shell (HevFshConfig *config)
{
    const char *sh = "/bin/sh";
    const char *bash = "/bin/bash";
    const char *cmd = bash;

    if (access (bash, X_OK) < 0)
        cmd = sh;

    if (getuid () == 0) {
        const char *user;

        user = hev_fsh_config_get_user (config);
        if (user) {
            struct passwd *pwd;

            pwd = getpwnam (user);
            if (pwd) {
                if (setgid (pwd->pw_gid)) {
                    /* ignore return value */
                }
                if (setuid (pwd->pw_uid)) {
                    /* ignore return value */
                }
            }
        } else {
            setsid ();
            cmd = "/bin/login";
        }
    }

    if (!getenv ("TERM"))
        setenv ("TERM", "linux", 1);

    execl (cmd, cmd, NULL);
    exit (0);
}

static void
forkpty_entry (HevTaskCall *call)
{
    HevTaskCallForkPty *fpty = (HevTaskCallForkPty *)call;
    pid_t pid;

    pid = forkpty (fpty->pfd, NULL, NULL, NULL);
    if (pid < 0) {
        hev_task_call_set_retval (call, NULL);
    } else if (pid == 0) {
        exec_shell (fpty->config);
    } else {
        struct winsize win_size;

        win_size.ws_row = fpty->term_info->rows;
        win_size.ws_col = fpty->term_info->columns;
        win_size.ws_xpixel = 0;
        win_size.ws_ypixel = 0;

        ioctl (*fpty->pfd, TIOCSWINSZ, &win_size);
        hev_task_call_set_retval (call, fpty);
    }
}

static void
hev_fsh_client_term_accept_task_entry (void *data)
{
    HevFshClientTermAccept *self = data;
    HevFshClientBase *base = data;
    HevFshMessageTermInfo mtinfo;
    HevTaskCallForkPty *fpty;
    HevTaskCall *call;
    void *ptr;
    int sfd;
    int pfd;
    int res;

    res = hev_fsh_client_accept_send_accept (&self->base);
    if (res < 0)
        goto quit;

    sfd = base->fd;
    /* recv msg term info */
    res = hev_task_io_socket_recv (sfd, &mtinfo, sizeof (mtinfo), MSG_WAITALL,
                                   io_yielder, self);
    if (res != sizeof (mtinfo))
        goto quit;

    call = hev_task_call_new (sizeof (HevTaskCallForkPty), 16384);
    if (!call)
        goto quit;

    fpty = (HevTaskCallForkPty *)call;
    fpty->pfd = &pfd;
    fpty->term_info = &mtinfo;
    fpty->config = base->config;

    ptr = hev_task_call_jump (call, forkpty_entry);
    hev_task_call_destroy (call);
    if (!ptr)
        goto quit;

    res = fcntl (pfd, F_SETFL, O_NONBLOCK);
    if (res < 0)
        goto quit_close;

    hev_task_add_fd (hev_task_self (), pfd, POLLIN | POLLOUT);

    if (hev_fsh_config_is_ugly_ktls (base->config))
        hev_task_io_us_splice (sfd, sfd, pfd, pfd, 8192, io_yielder, self);
    else
        hev_task_io_splice (sfd, sfd, pfd, pfd, 8192, io_yielder, self);

quit_close:
    hev_task_io_close (pfd);
quit:
    hev_object_unref (HEV_OBJECT (self));
}

#endif

static void
hev_fsh_client_term_accept_run (HevFshIO *base)
{
    LOG_D ("%p fsh client term accept run", base);

    hev_task_run (base->task, hev_fsh_client_term_accept_task_entry, base);
}

HevFshClientBase *
hev_fsh_client_term_accept_new (HevFshConfig *config, HevFshToken token)
{
    HevFshClientTermAccept *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClientTermAccept));
    if (!self)
        return NULL;

    res = hev_fsh_client_term_accept_construct (self, config, token);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client term accept new", self);

    return HEV_FSH_CLIENT_BASE (self);
}

int
hev_fsh_client_term_accept_construct (HevFshClientTermAccept *self,
                                      HevFshConfig *config, HevFshToken token)
{
    int res;

    res = hev_fsh_client_accept_construct (&self->base, config, token);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client term accept construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_TERM_ACCEPT_TYPE;

    return 0;
}

static void
hev_fsh_client_term_accept_destruct (HevObject *base)
{
    HevFshClientTermAccept *self = HEV_FSH_CLIENT_TERM_ACCEPT (base);

    LOG_D ("%p fsh client term accept destruct", self);

    HEV_FSH_CLIENT_ACCEPT_TYPE->destruct (base);
}

HevObjectClass *
hev_fsh_client_term_accept_class (void)
{
    static HevFshClientTermAcceptClass klass;
    HevFshClientTermAcceptClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshIOClass *ikptr;
        void *ptr;

        ptr = HEV_FSH_CLIENT_ACCEPT_TYPE;
        memcpy (kptr, ptr, sizeof (HevFshClientAcceptClass));

        okptr->name = "HevFshClientTermAccept";
        okptr->destruct = hev_fsh_client_term_accept_destruct;

        ikptr = HEV_FSH_IO_CLASS (kptr);
        ikptr->run = hev_fsh_client_term_accept_run;
    }

    return okptr;
}
