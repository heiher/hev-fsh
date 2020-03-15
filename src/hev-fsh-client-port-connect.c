/*
 ============================================================================
 Name        : hev-fsh-client-port-connect.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client port connect
 ============================================================================
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-fsh-protocol.h"

#include "hev-fsh-client-port-connect.h"

#define fsh_task_io_yielder hev_fsh_session_task_io_yielder

struct _HevFshClientPortConnect
{
    HevFshClientConnect base;

    int local_fd;
};

static void hev_fsh_client_port_connect_task_entry (void *data);
static void hev_fsh_client_port_connect_destroy (HevFshClientConnect *base);

HevFshClientBase *
hev_fsh_client_port_connect_new (HevFshConfig *config, int local_fd,
                                 HevFshSessionManager *sm)
{
    HevFshClientPortConnect *self;
    HevFshSession *s;

    self = hev_malloc0 (sizeof (HevFshClientPortConnect));
    if (!self) {
        fprintf (stderr, "Allocate client port connect failed!\n");
        goto exit;
    }

    if (hev_fsh_client_connect_construct (&self->base, config, sm) < 0) {
        fprintf (stderr, "Construct client connect failed!\n");
        goto exit_free;
    }

    s = HEV_FSH_SESSION (self);
    hev_task_run (s->task, hev_fsh_client_port_connect_task_entry, self);

    self->local_fd = local_fd;
    self->base._destroy = hev_fsh_client_port_connect_destroy;
    return &self->base.base;

exit_free:
    hev_free (self);
exit:
    return NULL;
}

static void
hev_fsh_client_port_connect_destroy (HevFshClientConnect *base)
{
    HevFshClientPortConnect *self = HEV_FSH_CLIENT_PORT_CONNECT (base);

    if (self->local_fd >= 0)
        close (self->local_fd);
    hev_free (self);
}

static void
hev_fsh_client_port_connect_task_entry (void *data)
{
    HevFshClientPortConnect *self = HEV_FSH_CLIENT_PORT_CONNECT (data);
    HevFshClientBase *base = HEV_FSH_CLIENT_BASE (self);
    HevTask *task = hev_task_self ();
    HevFshMessagePortInfo msg_pinfo;
    int ifd, ofd, port;
    const char *addr;

    if (hev_fsh_client_connect_send_connect (&self->base) < 0)
        goto exit;

    addr = hev_fsh_config_get_remote_address (base->config);
    port = hev_fsh_config_get_remote_port (base->config);
    msg_pinfo.type = 4;
    msg_pinfo.port = htons (port);
    inet_aton (addr, (struct in_addr *)&msg_pinfo.addr);

    /* send message port info */
    if (hev_task_io_socket_send (base->fd, &msg_pinfo, sizeof (msg_pinfo),
                                 MSG_WAITALL, fsh_task_io_yielder, self) <= 0)
        goto exit;

    if (self->local_fd < 0) {
        ifd = 0;
        ofd = 1;
    } else {
        ifd = self->local_fd;
        ofd = self->local_fd;
    }

    if (fcntl (ifd, F_SETFL, O_NONBLOCK) < 0)
        goto exit;

    if (ifd != ofd) {
        if (fcntl (ofd, F_SETFL, O_NONBLOCK) < 0)
            goto exit;

        hev_task_add_fd (task, ifd, POLLIN);
        hev_task_add_fd (task, ofd, POLLOUT);
    } else {
        hev_task_add_fd (task, ifd, POLLIN | POLLOUT);
    }

    hev_task_io_splice (base->fd, base->fd, ifd, ofd, 8192, fsh_task_io_yielder,
                        self);

exit:
    hev_fsh_client_base_destroy (base);
}
