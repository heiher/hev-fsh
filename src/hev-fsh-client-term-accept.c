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
#include <unistd.h>
#include <sys/ioctl.h>
#include <pwd.h>
#if defined(__linux__)
#include <pty.h>
#elif defined(__APPLE__) || (__MACH__)
#include <util.h>
#else
#include <termios.h>
#include <libutil.h>
#endif

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-call.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-task-io-us.h"
#include "hev-fsh-protocol.h"

#include "hev-fsh-client-term-accept.h"

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
    close (pfd);
quit:
    hev_object_unref (HEV_OBJECT (self));
}

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
