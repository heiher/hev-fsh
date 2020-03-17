/*
 ============================================================================
 Name        : hev-fsh-client-base.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client base
 ============================================================================
 */

#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>

#include "hev-fsh-client-base.h"

#define fsh_task_io_yielder hev_fsh_session_task_io_yielder

void
hev_fsh_client_base_init (HevFshClientBase *self, HevFshConfig *config,
                          HevFshSessionManager *sm)
{
    self->fd = -1;
    self->sm = sm;
    self->config = config;
    self->base.hp = HEV_FSH_SESSION_HP;
    hev_fsh_session_manager_insert (sm, &self->base);
    signal (SIGCHLD, SIG_IGN);
}

void
hev_fsh_client_base_destroy (HevFshClientBase *self)
{
    if (self->fd >= 0)
        close (self->fd);
    hev_fsh_session_manager_remove (self->sm, &self->base);

    if (self->_destroy)
        self->_destroy (self);
}

static int
client_base_socket (int family)
{
    int fd, flags;

    fd = hev_task_io_socket_socket (family, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
        goto exit;

    flags = fcntl (fd, F_GETFD);
    if (flags < 0)
        goto exit_close;

    flags |= FD_CLOEXEC;
    if (fcntl (fd, F_SETFD, flags) < 0)
        goto exit_close;

    return fd;

exit_close:
    close (fd);
exit:
    return -1;
}

int
hev_fsh_client_base_listen (HevFshClientBase *self)
{
    struct sockaddr *addr;
    socklen_t addr_len;
    int one = 1;

    addr = hev_fsh_config_get_local_sockaddr (self->config, &addr_len);
    self->fd = client_base_socket (addr->sa_family);
    if (self->fd < 0)
        return -1;

    if (setsockopt (self->fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one)) < 0)
        return -1;

    if (bind (self->fd, addr, addr_len) < 0)
        return -1;

    if (listen (self->fd, 10) < 0)
        return -1;

    hev_task_add_fd (hev_task_self (), self->fd, POLLIN);

    return 0;
}

int
hev_fsh_client_base_connect (HevFshClientBase *self)
{
    struct sockaddr *addr;
    socklen_t addr_len;

    addr = hev_fsh_config_get_server_sockaddr (self->config, &addr_len);
    self->fd = client_base_socket (addr->sa_family);
    if (self->fd < 0)
        return -1;

    hev_task_add_fd (hev_task_self (), self->fd, POLLIN | POLLOUT);

    if (hev_task_io_socket_connect (self->fd, addr, addr_len,
                                    fsh_task_io_yielder, self) < 0)
        return -1;

    return 0;
}
