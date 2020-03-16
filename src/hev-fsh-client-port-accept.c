/*
 ============================================================================
 Name        : hev-fsh-client-port-accept.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client port accept
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-fsh-client-port-accept.h"

#define fsh_task_io_yielder hev_fsh_session_task_io_yielder

struct _HevFshClientPortAccept
{
    HevFshClientAccept base;
};

static void hev_fsh_client_port_accept_task_entry (void *data);
static void hev_fsh_client_port_accept_destroy (HevFshClientAccept *base);

HevFshClientBase *
hev_fsh_client_port_accept_new (HevFshConfig *config, HevFshToken token,
                                HevFshSessionManager *sm)
{
    HevFshClientPortAccept *self;
    HevFshSession *s;

    self = hev_malloc0 (sizeof (HevFshClientPortAccept));
    if (!self) {
        fprintf (stderr, "Allocate client port accept failed!\n");
        goto exit;
    }

    if (hev_fsh_client_accept_construct (&self->base, config, token, sm) < 0)
        goto exit_free;

    s = HEV_FSH_SESSION (self);
    hev_task_run (s->task, hev_fsh_client_port_accept_task_entry, self);

    self->base._destroy = hev_fsh_client_port_accept_destroy;
    return &self->base.base;

exit_free:
    hev_free (self);
exit:
    return NULL;
}

static void
hev_fsh_client_port_accept_destroy (HevFshClientAccept *base)
{
    hev_free (base);
}

static void
hev_fsh_client_port_accept_task_entry (void *data)
{
    HevFshClientPortAccept *self = HEV_FSH_CLIENT_PORT_ACCEPT (data);
    HevFshMessagePortInfo msg_pinfo;
    struct sockaddr_in6 addr;
    int rfd, lfd;

    rfd = self->base.base.fd;
    if (hev_fsh_client_accept_send_accept (&self->base) < 0)
        goto quit;

    /* recv message port info */
    if (hev_task_io_socket_recv (rfd, &msg_pinfo, sizeof (msg_pinfo),
                                 MSG_WAITALL, fsh_task_io_yielder, self) <= 0)
        goto quit;

    if (!hev_fsh_config_addr_list_contains (self->base.base.config,
                                            msg_pinfo.type, msg_pinfo.addr,
                                            msg_pinfo.port))
        goto quit;

    __builtin_bzero (&addr, sizeof (addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = msg_pinfo.port;

    switch (msg_pinfo.type) {
    case 4:
        memcpy (&addr.sin6_addr.s6_addr[12], msg_pinfo.addr, 4);
        ((uint16_t *)&addr.sin6_addr)[5] = 0xffff;
        break;
    case 6:
        memcpy (&addr.sin6_addr, msg_pinfo.addr, 16);
        break;
    default:
        goto quit;
    }

    lfd = hev_task_io_socket_socket (AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (lfd < 0)
        goto quit;

    hev_task_add_fd (hev_task_self (), lfd, POLLIN | POLLOUT);

    if (hev_task_io_socket_connect (lfd, (struct sockaddr *)&addr,
                                    sizeof (addr), fsh_task_io_yielder,
                                    self) < 0)
        goto quit_close_fd;

    hev_task_io_splice (rfd, rfd, lfd, lfd, 8192, fsh_task_io_yielder, self);

quit_close_fd:
    close (lfd);
quit:
    hev_fsh_client_base_destroy ((HevFshClientBase *)self);
}
