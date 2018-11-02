/*
 ============================================================================
 Name        : hev-fsh-client-port-forward.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client port forward
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "hev-fsh-client-port-forward.h"
#include "hev-fsh-client-port-accept.h"
#include "hev-fsh-protocol.h"
#include "hev-memory-allocator.h"
#include "hev-task.h"
#include "hev-task-io-socket.h"

#define TASK_STACK_SIZE (64 * 4096)
#define KEEP_ALIVE_INTERVAL (30 * 1000)

struct _HevFshClientPortForward
{
    HevFshClientBase base;

    HevFshToken token;

    HevTask *task;
    HevFshConfig *config;
};

static void hev_fsh_client_port_forward_task_entry (void *data);
static void hev_fsh_client_port_forward_destroy (HevFshClientBase *self);

HevFshClientBase *
hev_fsh_client_port_forward_new (HevFshConfig *config)
{
    HevFshClientPortForward *self;
    const char *address;
    unsigned int port;

    self = hev_malloc0 (sizeof (HevFshClientPortForward));
    if (!self) {
        fprintf (stderr, "Allocate client port forward failed!\n");
        return NULL;
    }

    address = hev_fsh_config_get_server_address (config);
    port = hev_fsh_config_get_server_port (config);

    if (0 > hev_fsh_client_base_construct (&self->base, address, port)) {
        fprintf (stderr, "Construct client base failed!\n");
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
    self->base._destroy = hev_fsh_client_port_forward_destroy;

    hev_task_run (self->task, hev_fsh_client_port_forward_task_entry, self);

    return &self->base;
}

static void
hev_fsh_client_port_forward_destroy (HevFshClientBase *self)
{
    hev_free (self);
}

static void
hev_fsh_client_port_forward_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientPortForward *self = data;
    HevFshMessage message;
    HevFshMessageToken msg_token;
    int sock_fd, wait_keep_alive = 0;
    unsigned int sleep_ms = KEEP_ALIVE_INTERVAL;
    const char *token, *token_src;
    char token_buf[40];
    ssize_t len;

    sock_fd = self->base.fd;
    hev_task_add_fd (task, sock_fd, EPOLLIN | EPOLLOUT);

    if (hev_task_io_socket_connect (sock_fd, &self->base.address,
                                    sizeof (struct sockaddr_in), NULL,
                                    NULL) < 0) {
        fprintf (stderr, "Connect to server failed!\n");
        return;
    }

    token = hev_fsh_config_get_token (self->config);

    message.ver = token ? 2 : 1;
    message.cmd = HEV_FSH_CMD_LOGIN;

    len = hev_task_io_socket_send (sock_fd, &message, sizeof (message),
                                   MSG_WAITALL, NULL, NULL);
    if (len <= 0)
        return;

    if (token) {
        if (0 > hev_fsh_protocol_token_from_string (self->token, token)) {
            fprintf (stderr, "Can't parse token!\n");
            return;
        }

        memcpy (msg_token.token, self->token, sizeof (HevFshToken));
        len = hev_task_io_socket_send (sock_fd, &msg_token, sizeof (msg_token),
                                       MSG_WAITALL, NULL, NULL);
        if (len <= 0)
            return;
    }

    /* recv message token */
    len = hev_task_io_socket_recv (sock_fd, &message, sizeof (message),
                                   MSG_WAITALL, NULL, NULL);
    if (len <= 0)
        return;

    if (message.cmd != HEV_FSH_CMD_TOKEN) {
        fprintf (stderr, "Can't login to server!\n");
        return;
    }

    len = hev_task_io_socket_recv (sock_fd, &msg_token, sizeof (msg_token),
                                   MSG_WAITALL, NULL, NULL);
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

    for (;;) {
        sleep_ms = hev_task_sleep (sleep_ms);

        len = recv (sock_fd, &message, sizeof (message), MSG_PEEK);
        if (len == -1 && errno == EAGAIN) {
            /* timeout */
            if (0 == sleep_ms && wait_keep_alive) {
                printf ("Connection lost!\n");
                return;
            }
            /* keep alive */
            message.ver = 2;
            message.cmd = HEV_FSH_CMD_KEEP_ALIVE;
            len = hev_task_io_socket_send (sock_fd, &message, sizeof (message),
                                           MSG_WAITALL, NULL, NULL);
            if (len <= 0)
                return;
            wait_keep_alive = 1;
            sleep_ms = KEEP_ALIVE_INTERVAL;
            continue;
        }

        /* recv message connect */
        len = hev_task_io_socket_recv (sock_fd, &message, sizeof (message),
                                       MSG_WAITALL, NULL, NULL);
        if (len <= 0)
            return;

        switch (message.cmd) {
        case HEV_FSH_CMD_KEEP_ALIVE:
            wait_keep_alive = 0;
            sleep_ms = KEEP_ALIVE_INTERVAL;
            continue;
        case HEV_FSH_CMD_CONNECT:
            break;
        default:
            return;
        }

        len = hev_task_io_socket_recv (sock_fd, &msg_token, sizeof (msg_token),
                                       MSG_WAITALL, NULL, NULL);
        if (len <= 0)
            return;

        if (0 != memcmp (msg_token.token, self->token, sizeof (HevFshToken)))
            return;

        hev_fsh_client_port_accept_new (self->config, self->token);
        sleep_ms = KEEP_ALIVE_INTERVAL;
    }
}
