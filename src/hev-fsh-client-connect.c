/*
 ============================================================================
 Name        : hev-fsh-client-connect.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client connect
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-fsh-protocol.h"

#include "hev-fsh-client-connect.h"

#define fsh_task_io_yielder hev_fsh_session_task_io_yielder

static void hev_fsh_client_connect_destroy (HevFshClientBase *base);

int
hev_fsh_client_connect_construct (HevFshClientConnect *self,
                                  HevFshConfig *config,
                                  HevFshSessionManager *sm)
{
    HevFshSession *s = HEV_FSH_SESSION (self);

    if (hev_fsh_client_base_construct (&self->base, config, sm) < 0) {
        fprintf (stderr, "Construct client base failed!\n");
        goto exit;
    }

    s->task = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!s->task) {
        fprintf (stderr, "Create client connect's task failed!\n");
        goto exit_free;
    }

    self->base._destroy = hev_fsh_client_connect_destroy;
    return 0;

exit_free:
    hev_fsh_client_base_destroy (&self->base);
exit:
    return -1;
}

static void
hev_fsh_client_connect_destroy (HevFshClientBase *base)
{
    HevFshClientConnect *self = HEV_FSH_CLIENT_CONNECT (base);

    if (self->_destroy)
        self->_destroy (self);
}

int
hev_fsh_client_connect_send_connect (HevFshClientConnect *self)
{
    HevFshMessage msg;
    HevFshMessageToken msg_token;
    struct sockaddr_in saddr;
    struct sockaddr *addr;
    const char *token;

    hev_task_add_fd (hev_task_self (), self->base.fd, POLLIN | POLLOUT);

    msg.ver = 1;
    msg.cmd = HEV_FSH_CMD_CONNECT;
    addr = (struct sockaddr *)&saddr;
    token = hev_fsh_config_get_token (self->base.config);

    if (hev_fsh_client_base_resolv (&self->base, addr) < 0) {
        fprintf (stderr, "Resolv server address failed!\n");
        return -1;
    }

    if (hev_task_io_socket_connect (self->base.fd, addr, sizeof (saddr),
                                    fsh_task_io_yielder, self) < 0) {
        fprintf (stderr, "Connect to server failed!\n");
        return -1;
    }

    if (hev_task_io_socket_send (self->base.fd, &msg, sizeof (msg), MSG_WAITALL,
                                 fsh_task_io_yielder, self) <= 0)
        return -1;

    if (hev_fsh_protocol_token_from_string (msg_token.token, token) == -1) {
        fprintf (stderr, "Can't parse token!\n");
        return -1;
    }

    if (hev_task_io_socket_send (self->base.fd, &msg_token, sizeof (msg_token),
                                 MSG_WAITALL, fsh_task_io_yielder, self) <= 0)
        return -1;

    return 0;
}
