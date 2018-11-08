/*
 ============================================================================
 Name        : hev-fsh-client-port-accept.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client port accept
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "hev-fsh-client-port-accept.h"
#include "hev-memory-allocator.h"
#include "hev-task-io.h"
#include "hev-task-io-socket.h"

struct _HevFshClientPortAccept
{
    HevFshClientAccept base;
};

static void hev_fsh_client_port_accept_task_entry (void *data);
static void hev_fsh_client_port_accept_destroy (HevFshClientAccept *base);

HevFshClientBase *
hev_fsh_client_port_accept_new (HevFshConfig *config, HevFshToken token)
{
    HevFshClientPortAccept *self;

    self = hev_malloc0 (sizeof (HevFshClientPortAccept));
    if (!self) {
        fprintf (stderr, "Allocate client port accept failed!\n");
        return NULL;
    }

    if (0 > hev_fsh_client_accept_construct (&self->base, config, token)) {
        hev_free (self);
        return NULL;
    }

    self->base._destroy = hev_fsh_client_port_accept_destroy;

    hev_task_run (self->base.task, hev_fsh_client_port_accept_task_entry, self);

    return &self->base.base;
}

static void
hev_fsh_client_port_accept_destroy (HevFshClientAccept *base)
{
    hev_free (base);
}

static void
hev_fsh_client_port_accept_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientPortAccept *self = data;
    HevFshMessagePortInfo message_port_info;
    int rfd, lfd;
    struct sockaddr_in addr;

    rfd = self->base.base.fd;
    if (0 > hev_fsh_client_accept_send_accept (&self->base))
        goto quit;

    /* recv message port info */
    if (0 > hev_task_io_socket_recv (rfd, &message_port_info,
                                     sizeof (message_port_info), MSG_WAITALL,
                                     NULL, NULL))
        goto quit;

    if (!hev_fsh_config_addr_list_contains (
            self->base.config, message_port_info.type, message_port_info.addr,
            message_port_info.port))
        goto quit;

    switch (message_port_info.type) {
    case 4:
        addr.sin_family = AF_INET;
        memcpy (&addr.sin_addr, message_port_info.addr, 4);
        break;
    default:
        goto quit;
    }
    addr.sin_port = message_port_info.port;

    lfd = socket (AF_INET, SOCK_STREAM, 0);
    if (0 > lfd)
        goto quit;

    if (fcntl (lfd, F_SETFL, O_NONBLOCK) == -1)
        goto quit_close_fd;

    hev_task_add_fd (task, lfd, EPOLLIN | EPOLLOUT);

    if (0 > hev_task_io_socket_connect (lfd, (struct sockaddr *)&addr,
                                        sizeof (addr), NULL, NULL))
        goto quit_close_fd;

    hev_task_io_splice (rfd, rfd, lfd, lfd, 2048, NULL, NULL);

quit_close_fd:
    close (lfd);
quit:
    hev_fsh_client_base_destroy ((HevFshClientBase *)self);
}
