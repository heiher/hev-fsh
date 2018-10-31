/*
 ============================================================================
 Name        : hev-fsh-client-base.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client base
 ============================================================================
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>

#include "hev-fsh-client-base.h"

static int
hev_fsh_client_base_socket (void)
{
    int fd, flags;

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return -1;

    if (fcntl (fd, F_SETFL, O_NONBLOCK) == -1) {
        close (fd);
        return -1;
    }

    flags = fcntl (fd, F_GETFD);
    if (flags == -1) {
        close (fd);
        return -1;
    }

    flags |= FD_CLOEXEC;
    if (fcntl (fd, F_SETFD, flags) == -1) {
        close (fd);
        return -1;
    }

    return fd;
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

int
hev_fsh_client_base_construct_with_sockaddr (HevFshClientBase *self,
                                             const struct sockaddr *addr,
                                             socklen_t addrlen)
{
    self->fd = hev_fsh_client_base_socket ();
    if (self->fd == -1) {
        fprintf (stderr, "Create client's socket failed!\n");
        return -1;
    }

    memcpy (&self->address, addr, addrlen);

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
