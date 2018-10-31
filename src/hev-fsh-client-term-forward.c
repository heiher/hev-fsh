/*
 ============================================================================
 Name        : hev-fsh-client-term-forward.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client term forward
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "hev-fsh-client-term-forward.h"
#include "hev-fsh-client-term-accept.h"
#include "hev-fsh-protocol.h"
#include "hev-memory-allocator.h"
#include "hev-task.h"
#include "hev-task-io-socket.h"

#define TASK_STACK_SIZE (64 * 4096)
#define KEEP_ALIVE_INTERVAL (30 * 1000)

struct _HevFshClientTermForward
{
    HevFshClientBase base;

    const char *user;
    const char *token;

    HevTask *task;
};

static void hev_fsh_client_term_forward_task_entry (void *data);
static void hev_fsh_client_term_forward_destroy (HevFshClientBase *self);

HevFshClientTermForward *
hev_fsh_client_term_forward_new (const char *address, unsigned int port,
                                 const char *user, const char *token)
{
    HevFshClientTermForward *self;

    self = hev_malloc0 (sizeof (HevFshClientTermForward));
    if (!self) {
        fprintf (stderr, "Allocate client term forward failed!\n");
        return NULL;
    }

    if (0 > hev_fsh_client_base_construct (&self->base, address, port)) {
        fprintf (stderr, "Construct client base failed!\n");
        hev_free (self);
        return NULL;
    }

    self->task = hev_task_new (TASK_STACK_SIZE);
    if (!self->task) {
        fprintf (stderr, "Create client term's task failed!\n");
        hev_free (self);
        return NULL;
    }

    self->user = user;
    self->token = token;
    self->base._destroy = hev_fsh_client_term_forward_destroy;

    hev_task_run (self->task, hev_fsh_client_term_forward_task_entry, self);

    return self;
}

static void
hev_fsh_client_term_forward_destroy (HevFshClientBase *self)
{
    hev_free (self);
}

static void
hev_fsh_client_term_forward_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientTermForward *self = data;
    HevFshMessage message;
    HevFshMessageToken send_token;
    HevFshMessageToken recv_token;
    HevFshToken token;
    int sock_fd;
    ssize_t len;
    char token_str[40];
    char *token_src;

    sock_fd = self->base.fd;
    hev_task_add_fd (task, sock_fd, EPOLLIN | EPOLLOUT);

    if (hev_task_io_socket_connect (sock_fd, &self->base.address,
                                    sizeof (struct sockaddr_in), NULL,
                                    NULL) < 0) {
        fprintf (stderr, "Connect to server failed!\n");
        return;
    }

    message.ver = self->token ? 2 : 1;
    message.cmd = HEV_FSH_CMD_LOGIN;

    len = hev_task_io_socket_send (sock_fd, &message, sizeof (message),
                                   MSG_WAITALL, NULL, NULL);
    if (len <= 0)
        return;

    if (self->token) {
        if (hev_fsh_protocol_token_from_string (send_token.token,
                                                self->token) == -1) {
            fprintf (stderr, "Can't parse token!\n");
            return;
        }

        len = hev_task_io_socket_send (
            sock_fd, &send_token, sizeof (send_token), MSG_WAITALL, NULL, NULL);
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

    len = hev_task_io_socket_recv (sock_fd, &recv_token, sizeof (recv_token),
                                   MSG_WAITALL, NULL, NULL);
    if (len <= 0)
        return;

    memcpy (token, recv_token.token, sizeof (HevFshToken));
    hev_fsh_protocol_token_to_string (token, token_str);
    if (0 == memcmp (&send_token, &recv_token, sizeof (HevFshMessageToken))) {
        token_src = "client";
    } else {
        token_src = "server";
    }
    printf ("Token: %s (from %s)\n", token_str, token_src);

    for (;;) {
        hev_task_sleep (KEEP_ALIVE_INTERVAL);

        len = recv (sock_fd, &message, sizeof (message), MSG_PEEK);
        if (len == -1 && errno == EAGAIN) {
            /* keep alive */
            message.ver = 1;
            message.cmd = HEV_FSH_CMD_KEEP_ALIVE;
            len = hev_task_io_socket_send (sock_fd, &message, sizeof (message),
                                           MSG_WAITALL, NULL, NULL);
            if (len <= 0)
                return;
            continue;
        }

        /* recv message connect */
        len = hev_task_io_socket_recv (sock_fd, &message, sizeof (message),
                                       MSG_WAITALL, NULL, NULL);
        if (len <= 0)
            return;

        if (message.cmd != HEV_FSH_CMD_CONNECT)
            return;

        len = hev_task_io_socket_recv (
            sock_fd, &recv_token, sizeof (recv_token), MSG_WAITALL, NULL, NULL);
        if (len <= 0)
            return;

        if (memcmp (recv_token.token, token, sizeof (HevFshToken)) != 0)
            return;

        hev_fsh_client_term_accept_new (&self->base.address,
                                        sizeof (struct sockaddr_in), self->user,
                                        &recv_token.token);
    }
}
