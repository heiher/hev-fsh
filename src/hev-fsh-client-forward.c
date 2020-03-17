/*
 ============================================================================
 Name        : hev-fsh-client-forward.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client forward
 ============================================================================
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-fsh-protocol.h"
#include "hev-fsh-client-term-accept.h"
#include "hev-fsh-client-port-accept.h"

#include "hev-fsh-client-forward.h"

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

    hev_fsh_client_base_init (&self->base, config, sm);

    s = HEV_FSH_SESSION (self);
    s->task = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!s->task) {
        fprintf (stderr, "Create client forward's task failed!\n");
        goto exit_free;
    }

    self->notify = notify;
    self->notify_data = notify_data;
    self->base._destroy = hev_fsh_client_forward_destroy;
    hev_task_run (s->task, hev_fsh_client_forward_task_entry, self);
    return &self->base;

exit_free:
    hev_fsh_client_base_destroy (&self->base);
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
    HevFshSession *s = HEV_FSH_SESSION (data);

    s->hp = HEV_FSH_SESSION_HP / 2;
    hev_task_yield (type);

    return (s->hp > 0) ? 0 : -1;
}

static int
write_login (HevFshClientForward *self)
{
    HevFshMessage msg;
    const char *token = hev_fsh_config_get_token (self->base.config);

    msg.ver = token ? 2 : 1;
    msg.cmd = HEV_FSH_CMD_LOGIN;
    if (hev_task_io_socket_send (self->base.fd, &msg, sizeof (msg), MSG_WAITALL,
                                 fsh_task_io_yielder, self) <= 0)
        return -1;

    if (token) {
        HevFshMessageToken msg_token;

        if (hev_fsh_protocol_token_from_string (self->token, token) < 0) {
            fprintf (stderr, "Can't parse token!\n");
            return -1;
        }

        memcpy (msg_token.token, self->token, sizeof (HevFshToken));

        if (hev_task_io_socket_send (self->base.fd, &msg_token,
                                     sizeof (msg_token), MSG_WAITALL,
                                     fsh_task_io_yielder, self) <= 0)
            return -1;
    }

    return 0;
}

static int
read_token (HevFshClientForward *self)
{
    HevFshMessage msg;
    HevFshMessageToken msg_token;
    const char *src;
    char buf[40];

    if (hev_task_io_socket_recv (self->base.fd, &msg, sizeof (msg), MSG_WAITALL,
                                 fsh_task_io_yielder, self) <= 0)
        return -1;

    if (msg.cmd != HEV_FSH_CMD_TOKEN) {
        fprintf (stderr, "Can't login to server!\n");
        return -1;
    }

    if (hev_task_io_socket_recv (self->base.fd, &msg_token, sizeof (msg_token),
                                 MSG_WAITALL, fsh_task_io_yielder, self) <= 0)
        return -1;

    hev_fsh_protocol_token_to_string (msg_token.token, buf);
    if (memcmp (msg_token.token, self->token, sizeof (HevFshToken)) == 0) {
        src = "client";
    } else {
        src = "server";
        memcpy (self->token, msg_token.token, sizeof (HevFshToken));
    }

    printf ("Token: %s (from %s)\n", buf, src);
    return 0;
}

static int
write_keep_alive (HevFshClientForward *self)
{
    HevFshMessage msg;

    msg.ver = 2;
    msg.cmd = HEV_FSH_CMD_KEEP_ALIVE;

    return hev_task_io_socket_send (self->base.fd, &msg, sizeof (msg),
                                    MSG_WAITALL, fsh_task_io_yielder, self);
}

static void
hev_fsh_client_forward_task_entry (void *data)
{
    HevFshClientForward *self = HEV_FSH_CLIENT_FORWARD (data);
    HevFshClientBase *base = HEV_FSH_CLIENT_BASE (data);
    int keep_alive = 0;

    if (hev_fsh_client_base_connect (base) < 0) {
        fprintf (stderr, "Connect to server failed!\n");
        goto exit;
    }

    if (write_login (self) < 0)
        goto exit;

    if (read_token (self) < 0)
        goto exit;

    for (;;) {
        HevFshMessageToken msg_token;
        HevFshMessage msg;
        ssize_t len;
        int mode;

        len = hev_task_io_socket_recv (base->fd, &msg, sizeof (msg),
                                       MSG_WAITALL, task_io_yielder, self);
        if (len == -2) {
            if (keep_alive++)
                goto exit;
            if (write_keep_alive (self) < 0)
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

        if (hev_task_io_socket_recv (base->fd, &msg_token, sizeof (msg_token),
                                     MSG_WAITALL, fsh_task_io_yielder,
                                     self) <= 0)
            goto exit;

        if (memcmp (msg_token.token, self->token, sizeof (HevFshToken)) != 0)
            goto exit;

        mode = hev_fsh_config_get_mode (base->config);
        if (HEV_FSH_CONFIG_MODE_FORWARDER_PORT == mode)
            hev_fsh_client_port_accept_new (base->config, self->token,
                                            base->sm);
        else
            hev_fsh_client_term_accept_new (base->config, self->token,
                                            base->sm);

        if (write_keep_alive (self) < 0)
            goto exit;
    }

exit:
    if (self->notify) {
        hev_task_del_fd (hev_task_self (), base->fd);
        hev_task_sleep (1000);
        self->notify (&base->base, self->notify_data);
    }
    hev_fsh_client_base_destroy (base);
}
