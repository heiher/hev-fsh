/*
 ============================================================================
 Name        : hev-fsh-client-port-connect.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client port connect
 ============================================================================
 */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "hev-fsh-client-port-connect.h"
#include "hev-fsh-protocol.h"
#include "hev-memory-allocator.h"
#include "hev-task-io-socket.h"

struct _HevFshClientPortConnect
{
    HevFshClientConnect base;

    int local_fd;
};

static void hev_fsh_client_port_connect_task_entry (void *data);
static void hev_fsh_client_port_connect_destroy (HevFshClientConnect *base);

HevFshClientBase *
hev_fsh_client_port_connect_new (HevFshConfig *config, int local_fd)
{
    HevFshClientPortConnect *self;

    self = hev_malloc0 (sizeof (HevFshClientPortConnect));
    if (!self) {
        fprintf (stderr, "Allocate client port connect failed!\n");
        return NULL;
    }

    if (0 > hev_fsh_client_connect_construct (&self->base, config)) {
        fprintf (stderr, "Construct client connect failed!\n");
        hev_free (self);
        return NULL;
    }

    self->local_fd = local_fd;
    self->base._destroy = hev_fsh_client_port_connect_destroy;

    hev_task_run (self->base.task, hev_fsh_client_port_connect_task_entry,
                  self);

    return &self->base.base;
}

static void
hev_fsh_client_port_connect_destroy (HevFshClientConnect *base)
{
    HevFshClientPortConnect *self = (HevFshClientPortConnect *)base;

    close (self->local_fd);
    hev_free (self);
}

static void
hev_fsh_client_port_connect_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientPortConnect *self = data;
    HevFshMessagePortInfo message_port_info;
    const char *address;
    unsigned int port;
    ssize_t len;

    if (0 > hev_fsh_client_connect_send_connect (&self->base))
        goto quit;

    address = hev_fsh_config_get_remote_address (self->base.config);
    port = hev_fsh_config_get_remote_port (self->base.config);
    message_port_info.type = 4;
    message_port_info.port = htons (port);
    inet_aton (address, (struct in_addr *)&message_port_info.addr);

    /* send message port info */
    len = hev_task_io_socket_send (self->base.base.fd, &message_port_info,
                                   sizeof (message_port_info), MSG_WAITALL,
                                   NULL, NULL);
    if (len <= 0)
        goto quit;

    if (fcntl (self->local_fd, F_SETFL, O_NONBLOCK) == -1)
        goto quit;

    hev_task_add_fd (task, self->local_fd, EPOLLIN | EPOLLOUT);

    hev_task_io_splice (self->base.base.fd, self->base.base.fd, self->local_fd,
                        self->local_fd, 2048, NULL, NULL);

quit:
    hev_fsh_client_base_destroy ((HevFshClientBase *)self);
}
