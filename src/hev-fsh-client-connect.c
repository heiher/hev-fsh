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
#include <netinet/in.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-fsh-protocol.h"

#include "hev-fsh-client-connect.h"

#define TASK_STACK_SIZE (8192)
#define fsh_task_io_yielder hev_fsh_session_task_io_yielder

static void hev_fsh_client_connect_destroy (HevFshClientBase *base);

int
hev_fsh_client_connect_construct (HevFshClientConnect *self,
                                  HevFshConfig *config,
                                  HevFshSessionManager *sm)
{
    HevFshSession *s = (HevFshSession *)self;
    const char *addr;
    unsigned int port;

    addr = hev_fsh_config_get_server_address (config);
    port = hev_fsh_config_get_server_port (config);

    if (0 > hev_fsh_client_base_construct (&self->base, addr, port, sm)) {
        fprintf (stderr, "Construct client base failed!\n");
        goto exit;
    }

    s->task = hev_task_new (TASK_STACK_SIZE);
    if (!s->task) {
        fprintf (stderr, "Create client connect's task failed!\n");
        goto exit_free;
    }

    self->config = config;
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
    HevFshClientConnect *self = (HevFshClientConnect *)base;

    if (self->_destroy)
        self->_destroy (self);
}

int
hev_fsh_client_connect_send_connect (HevFshClientConnect *self)
{
    HevTask *task = hev_task_self ();
    HevFshMessage message;
    HevFshMessageToken message_token;
    const char *token;
    ssize_t len;

    hev_task_add_fd (task, self->base.fd, POLLIN | POLLOUT);

    if (hev_task_io_socket_connect (self->base.fd, &self->base.address,
                                    sizeof (struct sockaddr_in),
                                    fsh_task_io_yielder, self) < 0) {
        fprintf (stderr, "Connect to server failed!\n");
        return -1;
    }

    message.ver = 1;
    message.cmd = HEV_FSH_CMD_CONNECT;
    len = hev_task_io_socket_send (self->base.fd, &message, sizeof (message),
                                   MSG_WAITALL, fsh_task_io_yielder, self);
    if (len <= 0)
        return -1;

    token = hev_fsh_config_get_token (self->config);
    if (hev_fsh_protocol_token_from_string (message_token.token, token) == -1) {
        fprintf (stderr, "Can't parse token!\n");
        return -1;
    }

    /* send message token */
    len = hev_task_io_socket_send (self->base.fd, &message_token,
                                   sizeof (message_token), MSG_WAITALL,
                                   fsh_task_io_yielder, self);
    if (len <= 0)
        return -1;

    return 0;
}
