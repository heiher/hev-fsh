/*
 ============================================================================
 Name        : hev-fsh-client-term-accept.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client term accept
 ============================================================================
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
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

#include "hev-fsh-client-term-accept.h"

#define fsh_task_io_yielder hev_fsh_session_task_io_yielder

typedef struct _HevTaskCallForkPty HevTaskCallForkPty;

struct _HevFshClientTermAccept
{
    HevFshClientAccept base;
};

struct _HevTaskCallForkPty
{
    HevTaskCall base;

    int *pfd;
    HevFshConfig *config;
    HevFshMessageTermInfo *term_info;
};

static void hev_fsh_client_term_accept_task_entry (void *data);
static void hev_fsh_client_term_accept_destroy (HevFshClientAccept *base);

HevFshClientBase *
hev_fsh_client_term_accept_new (HevFshConfig *config, HevFshToken token,
                                HevFshSessionManager *sm)
{
    HevFshClientTermAccept *self;
    HevFshSession *s;

    self = hev_malloc0 (sizeof (HevFshClientTermAccept));
    if (!self) {
        fprintf (stderr, "Allocate client term accept failed!\n");
        goto exit;
    }

    if (hev_fsh_client_accept_construct (&self->base, config, token, sm) < 0)
        goto exit_free;

    s = HEV_FSH_SESSION (self);
    hev_task_run (s->task, hev_fsh_client_term_accept_task_entry, self);

    self->base._destroy = hev_fsh_client_term_accept_destroy;
    return &self->base.base;

exit_free:
    hev_free (self);
exit:
    return NULL;
}

static void
hev_fsh_client_term_accept_destroy (HevFshClientAccept *base)
{
    hev_free (base);
}

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
    HevFshClientTermAccept *self = HEV_FSH_CLIENT_TERM_ACCEPT (data);
    HevFshClientBase *base = HEV_FSH_CLIENT_BASE (data);
    HevFshMessageTermInfo msg_tinfo;
    HevTaskCallForkPty *fpty;
    HevTaskCall *call;
    int sfd, pfd;
    void *ptr;

    if (hev_fsh_client_accept_send_accept (&self->base) < 0)
        goto quit;

    sfd = base->fd;
    /* recv msg term info */
    if (hev_task_io_socket_recv (sfd, &msg_tinfo, sizeof (msg_tinfo),
                                 MSG_WAITALL, fsh_task_io_yielder, self) <= 0)
        goto quit;

    call = hev_task_call_new (sizeof (HevTaskCallForkPty), 16384);
    if (!call)
        goto quit;

    fpty = (HevTaskCallForkPty *)call;
    fpty->pfd = &pfd;
    fpty->term_info = &msg_tinfo;
    fpty->config = base->config;

    ptr = hev_task_call_jump (call, forkpty_entry);
    hev_task_call_destroy (call);
    if (!ptr)
        goto quit;

    if (fcntl (pfd, F_SETFL, O_NONBLOCK) < 0)
        goto quit_close_fd;

    hev_task_add_fd (hev_task_self (), pfd, POLLIN | POLLOUT);
    hev_task_io_splice (sfd, sfd, pfd, pfd, 8192, fsh_task_io_yielder, self);

quit_close_fd:
    close (pfd);
quit:
    hev_fsh_client_base_destroy (base);
}
