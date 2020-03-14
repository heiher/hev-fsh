/*
 ============================================================================
 Name        : hev-fsh-client-forward.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client forward
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-fsh-protocol.h"
#include "hev-fsh-client-term-accept.h"
#include "hev-fsh-client-port-accept.h"

#include "hev-fsh-client-forward.h"

#define TASK_STACK_SIZE (16384)
#define fsh_task_io_yielder hev_fsh_session_task_io_yielder

struct _HevFshClientForward
{
    HevFshClientBase base;

    HevFshToken token;
    HevFshSessionNotify notify;
    void *notify_data;
};

static void hev_fsh_client_forward_task_entry (void *data);
static void hev_fsh_client_forward_destroy (HevFshClientBase *self);

HevFshClientBase *
hev_fsh_client_forward_new (HevFshConfig *config, HevFshSessionManager *sm,
                            HevFshSessionNotify notify, void *notify_data)
{
    HevFshClientForward *self;
    HevFshSession *s;

    self = hev_malloc0 (sizeof (HevFshClientForward));
    if (!self) {
        fprintf (stderr, "Allocate client forward failed!\n");
        goto exit;
    }

    if (hev_fsh_client_base_construct (&self->base, config, sm) < 0) {
        fprintf (stderr, "Construct client base failed!\n");
        goto exit_free;
    }

    s = (HevFshSession *)self;
    s->task = hev_task_new (TASK_STACK_SIZE);
    if (!s->task) {
        fprintf (stderr, "Create client forward's task failed!\n");
        goto exit_free_base;
    }

    self->notify = notify;
    self->notify_data = notify_data;
    self->base._destroy = hev_fsh_client_forward_destroy;

    hev_task_run (s->task, hev_fsh_client_forward_task_entry, self);

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

static int
task_io_yielder (HevTaskYieldType type, void *data)
{
    HevFshSession *s = data;

    s->hp = HEV_FSH_SESSION_HP / 2;
    hev_task_yield (type);

    return (s->hp > 0) ? 0 : -1;
}

static ssize_t
write_login (HevFshClientForward *self)
{
    HevFshMessage msg;
    const char *token = hev_fsh_config_get_token (self->base.config);
    ssize_t len;

    msg.ver = token ? 2 : 1;
    msg.cmd = HEV_FSH_CMD_LOGIN;
    len = hev_task_io_socket_send (self->base.fd, &msg, sizeof (msg),
                                   MSG_WAITALL, fsh_task_io_yielder, self);
    if (len <= 0)
        goto exit;

    if (token) {
        HevFshMessageToken msg_token;

        if (hev_fsh_protocol_token_from_string (self->token, token) < 0) {
            fprintf (stderr, "Can't parse token!\n");
            goto exit;
        }

        memcpy (msg_token.token, self->token, sizeof (HevFshToken));
        len = hev_task_io_socket_send (self->base.fd, &msg_token,
                                       sizeof (msg_token), MSG_WAITALL,
                                       fsh_task_io_yielder, self);
        if (len <= 0)
            goto exit;
    }

exit:
    return len;
}

static ssize_t
read_token (HevFshClientForward *self)
{
    HevFshMessage msg;
    HevFshMessageToken msg_token;
    const char *token_src;
    char token_buf[40];
    ssize_t len;

    len = hev_task_io_socket_recv (self->base.fd, &msg, sizeof (msg),
                                   MSG_WAITALL, fsh_task_io_yielder, self);
    if (len <= 0)
        goto exit;

    if (msg.cmd != HEV_FSH_CMD_TOKEN) {
        fprintf (stderr, "Can't login to server!\n");
        goto exit;
    }

    len = hev_task_io_socket_recv (self->base.fd, &msg_token,
                                   sizeof (msg_token), MSG_WAITALL,
                                   fsh_task_io_yielder, self);
    if (len <= 0)
        goto exit;

    hev_fsh_protocol_token_to_string (msg_token.token, token_buf);
    if (memcmp (msg_token.token, self->token, sizeof (HevFshToken)) == 0) {
        token_src = "client";
    } else {
        token_src = "server";
        memcpy (self->token, msg_token.token, sizeof (HevFshToken));
    }

    printf ("Token: %s (from %s)\n", token_buf, token_src);

exit:
    return len;
}

static ssize_t
write_keep_alive (HevFshClientForward *self)
{
    HevFshMessage msg;
    ssize_t len;

    msg.ver = 2;
    msg.cmd = HEV_FSH_CMD_KEEP_ALIVE;
    len = hev_task_io_socket_send (self->base.fd, &msg, sizeof (msg),
                                   MSG_WAITALL, fsh_task_io_yielder, self);
    return len;
}

static void
hev_fsh_client_forward_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientForward *self = data;
    struct sockaddr_in address;
    struct sockaddr *addr;
    int keep_alive = 0;

    hev_task_add_fd (task, self->base.fd, POLLIN | POLLOUT);

    addr = (struct sockaddr *)&address;
    if (hev_fsh_client_base_resolv (&self->base, addr) < 0) {
        fprintf (stderr, "Resolv server address failed!\n");
        goto exit;
    }

    if (hev_task_io_socket_connect (self->base.fd, addr, sizeof (address),
                                    fsh_task_io_yielder, self) < 0) {
        fprintf (stderr, "Connect to server failed!\n");
        goto exit;
    }

    if (write_login (self) <= 0)
        goto exit;

    if (read_token (self) <= 0)
        goto exit;

    for (;;) {
        HevFshMessage msg;
        HevFshMessageToken msg_token;
        int mode = hev_fsh_config_get_mode (self->base.config);
        ssize_t len;

        len = hev_task_io_socket_recv (self->base.fd, &msg, sizeof (msg),
                                       MSG_WAITALL, task_io_yielder, self);
        if (len == -2) {
            if (keep_alive++)
                goto exit;
            if (write_keep_alive (self) <= 0)
                goto exit;
            continue;
        } else if (len <= 0) {
            goto exit;
        }

        switch (msg.cmd) {
        case HEV_FSH_CMD_CONNECT:
            break;
        case HEV_FSH_CMD_KEEP_ALIVE:
            keep_alive = 0;
            continue;
        default:
            goto exit;
        }

        len = hev_task_io_socket_recv (self->base.fd, &msg_token,
                                       sizeof (msg_token), MSG_WAITALL,
                                       fsh_task_io_yielder, self);
        if (len <= 0)
            goto exit;

        if (memcmp (msg_token.token, self->token, sizeof (HevFshToken)) != 0)
            goto exit;

        if (HEV_FSH_CONFIG_MODE_FORWARDER_PORT == mode)
            hev_fsh_client_port_accept_new (self->base.config, self->token,
                                            self->base._sm);
        else
            hev_fsh_client_term_accept_new (self->base.config, self->token,
                                            self->base._sm);

        if (write_keep_alive (self) <= 0)
            goto exit;
    }

exit:
    if (self->notify) {
        hev_task_del_fd (task, self->base.fd);
        hev_task_sleep (1000);
        self->notify (&self->base.base, self->notify_data);
    }
    hev_fsh_client_base_destroy (&self->base);
}
