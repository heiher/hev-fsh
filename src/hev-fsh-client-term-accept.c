/*
 ============================================================================
 Name        : hev-fsh-client-term-accept.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client term accept
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <pwd.h>
#include <pty.h>

#include "hev-fsh-client-term-accept.h"
#include "hev-memory-allocator.h"
#include "hev-task.h"
#include "hev-task-io-socket.h"

#define TASK_STACK_SIZE (64 * 4096)

struct _HevFshClientTermAccept
{
    HevFshClientBase base;

    HevFshToken token;

    HevTask *task;
    HevFshConfig *config;
};

static void hev_fsh_client_term_accept_task_entry (void *data);
static void hev_fsh_client_term_accept_destroy (HevFshClientBase *self);

HevFshClientTermAccept *
hev_fsh_client_term_accept_new (HevFshConfig *config, HevFshToken token)
{
    HevFshClientTermAccept *self;
    const char *address;
    unsigned int port;

    self = hev_malloc0 (sizeof (HevFshClientTermAccept));
    if (!self) {
        fprintf (stderr, "Allocate client term accept failed!\n");
        return NULL;
    }

    address = hev_fsh_config_get_server_address (config);
    port = hev_fsh_config_get_server_port (config);

    if (0 > hev_fsh_client_base_construct (&self->base, address, port)) {
        fprintf (stderr, "Construct client base failed!\n");
        hev_free (self);
        return NULL;
    }

    self->task = hev_task_new (TASK_STACK_SIZE);
    if (!self->task) {
        fprintf (stderr, "Create client term's task failed!\n");
        hev_free (self);
        return NULL;
    }

    self->config = config;
    memcpy (self->token, token, sizeof (HevFshToken));
    self->base._destroy = hev_fsh_client_term_accept_destroy;

    hev_task_run (self->task, hev_fsh_client_term_accept_task_entry, self);

    return self;
}

static void
hev_fsh_client_term_accept_destroy (HevFshClientBase *self)
{
    hev_free (self);
}

static void
hev_fsh_client_term_accept_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientTermAccept *self = data;
    HevFshMessage message;
    HevFshMessageToken message_token;
    HevFshMessageTermInfo message_term_info;
    int sock_fd, ptm_fd;
    struct msghdr mh;
    struct iovec iov[2];
    struct winsize win_size;
    ssize_t len;
    pid_t pid;

    sock_fd = self->base.fd;
    hev_task_add_fd (task, sock_fd, EPOLLIN | EPOLLOUT);

    if (hev_task_io_socket_connect (sock_fd, &self->base.address,
                                    sizeof (struct sockaddr_in), NULL,
                                    NULL) < 0)
        goto quit;

    message.ver = 1;
    message.cmd = HEV_FSH_CMD_ACCEPT;
    memcpy (message_token.token, self->token, sizeof (HevFshToken));

    memset (&mh, 0, sizeof (mh));
    mh.msg_iov = iov;
    mh.msg_iovlen = 2;

    iov[0].iov_base = &message;
    iov[0].iov_len = sizeof (message);
    iov[1].iov_base = &message_token;
    iov[1].iov_len = sizeof (message_token);

    if (hev_task_io_socket_sendmsg (sock_fd, &mh, MSG_WAITALL, NULL, NULL) <= 0)
        goto quit;

    /* recv message term info */
    len = hev_task_io_socket_recv (sock_fd, &message_term_info,
                                   sizeof (message_term_info), MSG_WAITALL,
                                   NULL, NULL);
    if (len <= 0)
        goto quit;

    pid = forkpty (&ptm_fd, NULL, NULL, NULL);
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

            user = hev_fsh_config_get_user (self->config);
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

        ioctl (ptm_fd, TIOCSWINSZ, &win_size);
    }

    if (fcntl (ptm_fd, F_SETFL, O_NONBLOCK) == -1)
        goto quit_close_fd;

    hev_task_add_fd (task, ptm_fd, EPOLLIN | EPOLLOUT);

    hev_task_io_splice (sock_fd, sock_fd, ptm_fd, ptm_fd, 2048, NULL, NULL);

quit_close_fd:
    close (ptm_fd);
quit:
    hev_fsh_client_base_destroy ((HevFshClientBase *)self);
}
