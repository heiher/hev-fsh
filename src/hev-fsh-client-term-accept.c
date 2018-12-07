/*
 ============================================================================
 Name        : hev-fsh-client-term-accept.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client term accept
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
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

#include "hev-fsh-client-term-accept.h"
#include "hev-memory-allocator.h"
#include "hev-task-io.h"
#include "hev-task-io-socket.h"

struct _HevFshClientTermAccept
{
    HevFshClientAccept base;
};

static void hev_fsh_client_term_accept_task_entry (void *data);
static void hev_fsh_client_term_accept_destroy (HevFshClientAccept *base);

HevFshClientBase *
hev_fsh_client_term_accept_new (HevFshConfig *config, HevFshToken token)
{
    HevFshClientTermAccept *self;

    self = hev_malloc0 (sizeof (HevFshClientTermAccept));
    if (!self) {
        fprintf (stderr, "Allocate client term accept failed!\n");
        return NULL;
    }

    if (0 > hev_fsh_client_accept_construct (&self->base, config, token)) {
        hev_free (self);
        return NULL;
    }

    self->base._destroy = hev_fsh_client_term_accept_destroy;

    hev_task_run (self->base.task, hev_fsh_client_term_accept_task_entry, self);

    return &self->base.base;
}

static void
hev_fsh_client_term_accept_destroy (HevFshClientAccept *base)
{
    hev_free (base);
}

static void
hev_fsh_client_term_accept_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientTermAccept *self = data;
    HevFshMessageTermInfo message_term_info;
    int sfd, pfd;
    struct winsize win_size;
    ssize_t len;
    pid_t pid;

    sfd = self->base.base.fd;
    if (0 > hev_fsh_client_accept_send_accept (&self->base))
        goto quit;

    /* recv message term info */
    len = hev_task_io_socket_recv (sfd, &message_term_info,
                                   sizeof (message_term_info), MSG_WAITALL,
                                   NULL, NULL);
    if (len <= 0)
        goto quit;

    pid = forkpty (&pfd, NULL, NULL, NULL);
    if (pid == -1) {
        goto quit;
    } else if (pid == 0) {
        const char *sh = "/bin/sh";
        const char *bash = "/bin/bash";
        const char *cmd = bash;

        if (access (bash, X_OK) < 0)
            cmd = sh;

        if (getuid () == 0) {
            const char *user;

            user = hev_fsh_config_get_user (self->base.config);
            if (user) {
                struct passwd *pwd;

                pwd = getpwnam (user);
                if (pwd) {
                    setgid (pwd->pw_gid);
                    setuid (pwd->pw_uid);
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
    } else {
        win_size.ws_row = message_term_info.rows;
        win_size.ws_col = message_term_info.columns;
        win_size.ws_xpixel = 0;
        win_size.ws_ypixel = 0;

        ioctl (pfd, TIOCSWINSZ, &win_size);
    }

    if (fcntl (pfd, F_SETFL, O_NONBLOCK) == -1)
        goto quit_close_fd;

    hev_task_add_fd (task, pfd, POLLIN | POLLOUT);

    hev_task_io_splice (sfd, sfd, pfd, pfd, 2048, NULL, NULL);

quit_close_fd:
    close (pfd);
quit:
    hev_fsh_client_base_destroy ((HevFshClientBase *)self);
}
