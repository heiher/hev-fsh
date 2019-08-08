/*
 ============================================================================
 Name        : hev-fsh-client-forward.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2019 everyone.
 Description : Fsh client forward
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "hev-task.h"
#include "hev-task-io.h"
#include "hev-task-io-socket.h"
#include "hev-memory-allocator.h"
#include "hev-fsh-protocol.h"
#include "hev-fsh-client-term-accept.h"
#include "hev-fsh-client-port-accept.h"

#include "hev-fsh-client-forward.h"

#define TASK_STACK_SIZE (8192)
#define fsh_task_io_yielder hev_fsh_client_base_task_io_yielder

struct _HevFshClientForward
{
    HevFshClientBase base;

    HevFshToken token;

    HevTask *task;
    HevFshConfig *config;
};

static void hev_fsh_client_forward_task_entry (void *data);
static void hev_fsh_client_forward_destroy (HevFshClientBase *self);

HevFshClientBase *
hev_fsh_client_forward_new (HevFshConfig *config)
{
    HevFshClientForward *self;
    const char *address;
    unsigned int port;

    self = hev_malloc0 (sizeof (HevFshClientForward));
    if (!self) {
        fprintf (stderr, "Allocate client forward failed!\n");
        goto exit;
    }

    address = hev_fsh_config_get_server_address (config);
    port = hev_fsh_config_get_server_port (config);

    if (0 > hev_fsh_client_base_construct (&self->base, address, port)) {
        fprintf (stderr, "Construct client base failed!\n");
        goto exit_free;
    }

    self->task = hev_task_new (TASK_STACK_SIZE);
    if (!self->task) {
        fprintf (stderr, "Create client forward's task failed!\n");
        goto exit_free_base;
    }

    self->config = config;
    self->base._destroy = hev_fsh_client_forward_destroy;

    hev_task_run (self->task, hev_fsh_client_forward_task_entry, self);

    return &self->base;

exit_free_base:
    hev_fsh_client_base_destroy (&self->base);
exit_free:
    hev_free (self);
exit:
    return NULL;
}

static void
hev_fsh_client_forward_destroy (HevFshClientBase *self)
{
    hev_free (self);
}

static void
hev_fsh_client_forward_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientForward *self = data;
    HevFshMessage message;
    HevFshMessageToken msg_token;
    int mode, sock_fd;
    const char *token, *token_src;
    char token_buf[40];
    ssize_t len;

    sock_fd = self->base.fd;
    hev_task_add_fd (task, sock_fd, POLLIN | POLLOUT);

    if (hev_task_io_socket_connect (sock_fd, &self->base.address,
                                    sizeof (struct sockaddr_in),
                                    fsh_task_io_yielder, NULL) < 0) {
        fprintf (stderr, "Connect to server failed!\n");
        return;
    }

    token = hev_fsh_config_get_token (self->config);

    message.ver = token ? 2 : 1;
    message.cmd = HEV_FSH_CMD_LOGIN;

    len = hev_task_io_socket_send (sock_fd, &message, sizeof (message),
                                   MSG_WAITALL, fsh_task_io_yielder, NULL);
    if (len <= 0)
        return;

    if (token) {
        if (0 > hev_fsh_protocol_token_from_string (self->token, token)) {
            fprintf (stderr, "Can't parse token!\n");
            return;
        }

        memcpy (msg_token.token, self->token, sizeof (HevFshToken));
        len = hev_task_io_socket_send (sock_fd, &msg_token, sizeof (msg_token),
                                       MSG_WAITALL, fsh_task_io_yielder, NULL);
        if (len <= 0)
            return;
    }

    /* recv message token */
    len = hev_task_io_socket_recv (sock_fd, &message, sizeof (message),
                                   MSG_WAITALL, fsh_task_io_yielder, NULL);
    if (len <= 0)
        return;

    if (message.cmd != HEV_FSH_CMD_TOKEN) {
        fprintf (stderr, "Can't login to server!\n");
        return;
    }

    len = hev_task_io_socket_recv (sock_fd, &msg_token, sizeof (msg_token),
                                   MSG_WAITALL, fsh_task_io_yielder, NULL);
    if (len <= 0)
        return;

    hev_fsh_protocol_token_to_string (msg_token.token, token_buf);
    if (0 == memcmp (msg_token.token, self->token, sizeof (HevFshToken))) {
        token_src = "client";
    } else {
        token_src = "server";
        memcpy (self->token, msg_token.token, sizeof (HevFshToken));
    }
    printf ("Token: %s (from %s)\n", token_buf, token_src);

    mode = hev_fsh_config_get_mode (self->config);

    for (;;) {
        /* recv message connect */
        len = hev_task_io_socket_recv (sock_fd, &message, sizeof (message),
                                       MSG_WAITALL, fsh_task_io_yielder, NULL);
        if (len == -2) {
            /* keep alive */
            message.ver = 2;
            message.cmd = HEV_FSH_CMD_KEEP_ALIVE;
            len = hev_task_io_socket_send (sock_fd, &message, sizeof (message),
                                           MSG_WAITALL, fsh_task_io_yielder,
                                           NULL);
            if (len <= 0)
                return;
            len = hev_task_io_socket_recv (sock_fd, &message, sizeof (message),
                                           MSG_WAITALL, fsh_task_io_yielder,
                                           NULL);
            if ((len <= 0) || (HEV_FSH_CMD_KEEP_ALIVE != message.cmd))
                return;
            continue;
        } else if (len <= 0) {
            return;
        }

        len = hev_task_io_socket_recv (sock_fd, &msg_token, sizeof (msg_token),
                                       MSG_WAITALL, fsh_task_io_yielder, NULL);
        if (len <= 0)
            return;

        if (0 != memcmp (msg_token.token, self->token, sizeof (HevFshToken)))
            return;

        if (HEV_FSH_CONFIG_MODE_FORWARDER_PORT == mode)
            hev_fsh_client_port_accept_new (self->config, self->token);
        else
            hev_fsh_client_term_accept_new (self->config, self->token);
    }
}
