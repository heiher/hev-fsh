/*
 ============================================================================
 Name        : hev-fsh-client-base.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2019 everyone.
 Description : Fsh client base
 ============================================================================
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "hev-task.h"
#include "hev-task-io.h"
#include "hev-task-io-socket.h"

#include "hev-fsh-client-base.h"

#define TIMEOUT (30 * 1000)

static int
hev_fsh_client_base_socket (void)
{
    int fd, flags;

    fd = hev_task_io_socket_socket (AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        goto exit;

    flags = fcntl (fd, F_GETFD);
    if (flags == -1)
        goto exit_close;

    flags |= FD_CLOEXEC;
    if (fcntl (fd, F_SETFD, flags) == -1)
        goto exit_close;

    return fd;

exit_close:
    close (fd);
exit:
    return -1;
}

int
hev_fsh_client_base_construct (HevFshClientBase *self, const char *address,
                               unsigned int port)
{
    struct sockaddr_in *addr;

    self->fd = hev_fsh_client_base_socket ();
    if (self->fd == -1) {
        fprintf (stderr, "Create client's socket failed!\n");
        return -1;
    }

    addr = (struct sockaddr_in *)&self->address;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = inet_addr (address);
    addr->sin_port = htons (port);

    signal (SIGCHLD, SIG_IGN);

    return 0;
}

void
hev_fsh_client_base_destroy (HevFshClientBase *self)
{
    close (self->fd);
    if (self->_destroy)
        self->_destroy (self);
}

int
hev_fsh_client_base_task_io_yielder (HevTaskYieldType type, void *data)
{
    if (type == HEV_TASK_WAITIO) {
        if (0 == hev_task_sleep (TIMEOUT))
            return -1;
    } else {
        hev_task_yield (HEV_TASK_YIELD);
    }

    return 0;
}
