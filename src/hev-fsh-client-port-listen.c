/*
 ============================================================================
 Name        : hev-fsh-client-port-listen.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client port connect
 ============================================================================
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>

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

    self = hev_malloc0 (sizeof (HevFshClientPortListen));
    if (!self) {
        fprintf (stderr, "Allocate client port connect failed!\n");
        goto exit;
    }

    hev_fsh_client_base_init (&self->base, config, sm);

    s = HEV_FSH_SESSION (self);
    s->task = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!s->task) {
        fprintf (stderr, "Create client port's task failed!\n");
        goto exit_free;
    }

    self->base._destroy = hev_fsh_client_port_listen_destroy;
    hev_task_run (s->task, hev_fsh_client_port_listen_task_entry, self);
    return &self->base;

exit_free:
    hev_fsh_client_base_destroy (&self->base);
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
    HevFshClientPortListen *self = HEV_FSH_CLIENT_PORT_LISTEN (data);
    HevFshClientBase *base = HEV_FSH_CLIENT_BASE (data);

    if (hev_fsh_client_base_listen (base) < 0) {
        fprintf (stderr, "Listen client socket failed!\n");
        goto exit;
    }

    for (;;) {
        HevFshConfig *config = self->base.config;
        HevFshSessionManager *sm = self->base.sm;
        int fd;

        fd = hev_task_io_socket_accept (self->base.fd, NULL, NULL, NULL, NULL);
        if (fd < 0)
            continue;

        if (!hev_fsh_client_port_connect_new (config, fd, sm))
            close (fd);
    }

exit:
    hev_fsh_client_base_destroy (&self->base);
}
