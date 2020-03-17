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
    HevFshClientBase *base = HEV_FSH_CLIENT_BASE (data);
    HevFshMessagePortInfo msg_pinfo;
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int rfd, lfd;

    if (hev_fsh_client_accept_send_accept (&self->base) < 0)
        goto quit;

    rfd = base->fd;
    /* recv message port info */
    if (hev_task_io_socket_recv (rfd, &msg_pinfo, sizeof (msg_pinfo),
                                 MSG_WAITALL, fsh_task_io_yielder, self) <= 0)
        goto quit;

    if (!hev_fsh_config_addr_list_contains (base->config, msg_pinfo.type,
                                            msg_pinfo.addr, msg_pinfo.port))
        goto quit;

    switch (msg_pinfo.type) {
    case 4: {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;
        addr_len = sizeof (struct sockaddr_in);
        __builtin_bzero (addr4, addr_len);
        addr4->sin_family = AF_INET;
        addr4->sin_port = msg_pinfo.port;
        memcpy (&addr4->sin_addr, msg_pinfo.addr, sizeof (addr4->sin_addr));
        break;
    }
    case 6: {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;
        addr_len = sizeof (struct sockaddr_in6);
        __builtin_bzero (addr6, addr_len);
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = msg_pinfo.port;
        memcpy (&addr6->sin6_addr, msg_pinfo.addr, sizeof (addr6->sin6_addr));
        break;
    }
    default:
        goto quit;
    }

    lfd = hev_task_io_socket_socket (addr.ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (lfd < 0)
        goto quit;

    hev_task_add_fd (hev_task_self (), lfd, POLLIN | POLLOUT);

    if (hev_task_io_socket_connect (lfd, (struct sockaddr *)&addr, addr_len,
                                    fsh_task_io_yielder, self) < 0)
        goto quit_close_fd;

    hev_task_io_splice (rfd, rfd, lfd, lfd, 8192, fsh_task_io_yielder, self);

quit_close_fd:
    close (lfd);
quit:
    hev_fsh_client_base_destroy (base);
}
