/*
 ============================================================================
 Name        : hev-fsh-client-port-listen.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client port connect
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-fsh-client-port-connect.h"

#include "hev-fsh-client-port-listen.h"

struct _HevFshClientPortListen
{
    HevFshClientBase base;
};

static void hev_fsh_client_port_listen_task_entry (void *data);
static void hev_fsh_client_port_listen_destroy (HevFshClientBase *self);

HevFshClientBase *
hev_fsh_client_port_listen_new (HevFshConfig *config, HevFshSessionManager *sm)
{
    HevFshClientPortListen *self;
    HevFshSession *s;
    struct sockaddr_in address;
    int reuseaddr = 1;
    const char *addr;
    int port;

    self = hev_malloc0 (sizeof (HevFshClientPortListen));
    if (!self) {
        fprintf (stderr, "Allocate client port connect failed!\n");
        goto exit;
    }

    if (hev_fsh_client_base_construct (&self->base, config, sm) < 0) {
        fprintf (stderr, "Construct client base failed!\n");
        goto exit_free;
    }

    if (setsockopt (self->base.fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                    sizeof (reuseaddr)) < 0) {
        fprintf (stderr, "Set reuse address failed!\n");
        goto exit_free_base;
    }

    addr = hev_fsh_config_get_local_address (config);
    port = hev_fsh_config_get_local_port (config);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr (addr);
    address.sin_port = htons (port);

    if (bind (self->base.fd, (struct sockaddr *)&address,
              sizeof (struct sockaddr_in)) < 0) {
        fprintf (stderr, "Bind client address failed!\n");
        goto exit_free_base;
    }

    if (listen (self->base.fd, 5) < 0) {
        fprintf (stderr, "Listen client socket failed!\n");
        goto exit_free_base;
    }

    s = (HevFshSession *)self;
    s->task = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!s->task) {
        fprintf (stderr, "Create client port's task failed!\n");
        goto exit_free_base;
    }

    self->base._destroy = hev_fsh_client_port_listen_destroy;

    hev_task_run (s->task, hev_fsh_client_port_listen_task_entry, self);

    return &self->base;

exit_free_base:
    hev_fsh_client_base_destroy (&self->base);
exit_free:
    hev_free (self);
exit:
    return NULL;
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
        HevFshConfig *config = self->base.config;
        HevFshSessionManager *sm = self->base._sm;
        int fd;

        fd = hev_task_io_socket_accept (self->base.fd, NULL, NULL, NULL, NULL);
        if (fd < 0)
            continue;

        if (!hev_fsh_client_port_connect_new (config, fd, sm))
            close (fd);
    }

    hev_fsh_client_base_destroy (&self->base);
}
