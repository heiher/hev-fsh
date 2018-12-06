/*
 ============================================================================
 Name        : hev-fsh-client-port-listen.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client port connect
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "hev-fsh-client-port-listen.h"
#include "hev-fsh-client-port-connect.h"
#include "hev-memory-allocator.h"
#include "hev-task.h"
#include "hev-task-io.h"
#include "hev-task-io-socket.h"

#define TASK_STACK_SIZE (64 * 4096)

struct _HevFshClientPortListen
{
    HevFshClientBase base;

    HevTask *task;
    HevFshConfig *config;
};

static void hev_fsh_client_port_listen_task_entry (void *data);
static void hev_fsh_client_port_listen_destroy (HevFshClientBase *self);

HevFshClientBase *
hev_fsh_client_port_listen_new (HevFshConfig *config)
{
    HevFshClientPortListen *self;
    const char *address;
    unsigned int port;
    int reuseaddr = 1;

    self = hev_malloc0 (sizeof (HevFshClientPortListen));
    if (!self) {
        fprintf (stderr, "Allocate client port connect failed!\n");
        return NULL;
    }

    address = hev_fsh_config_get_local_address (config);
    port = hev_fsh_config_get_local_port (config);

    if (0 > hev_fsh_client_base_construct (&self->base, address, port)) {
        fprintf (stderr, "Construct client base failed!\n");
        hev_free (self);
        return NULL;
    }

    if (0 > setsockopt (self->base.fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                        sizeof (reuseaddr))) {
        fprintf (stderr, "Set reuse address failed!\n");
        hev_free (self);
        return NULL;
    }

    if (0 > bind (self->base.fd, &self->base.address,
                  sizeof (struct sockaddr_in))) {
        fprintf (stderr, "Bind client address failed!\n");
        hev_free (self);
        return NULL;
    }

    if (0 > listen (self->base.fd, 5)) {
        fprintf (stderr, "Listen client socket failed!\n");
        hev_free (self);
        return NULL;
    }

    self->task = hev_task_new (TASK_STACK_SIZE);
    if (!self->task) {
        fprintf (stderr, "Create client port's task failed!\n");
        hev_free (self);
        return NULL;
    }

    self->config = config;
    self->base._destroy = hev_fsh_client_port_listen_destroy;

    hev_task_run (self->task, hev_fsh_client_port_listen_task_entry, self);

    return &self->base;
}

static void
hev_fsh_client_port_listen_destroy (HevFshClientBase *self)
{
    hev_free (self);
}

static void
hev_fsh_client_port_listen_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientPortListen *self = data;

    hev_task_add_fd (task, self->base.fd, POLLIN);

    for (;;) {
        int fd;
        HevFshClientBase *client;

        fd = hev_task_io_socket_accept (self->base.fd, NULL, NULL, NULL, NULL);
        if (0 > fd)
            continue;

        client = hev_fsh_client_port_connect_new (self->config, fd);
        if (!client)
            close (fd);
    }
}
