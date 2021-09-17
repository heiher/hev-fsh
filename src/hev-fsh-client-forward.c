/*
 ============================================================================
 Name        : hev-fsh-client-forward.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client forward
 ============================================================================
 */

#include <string.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-fsh-client-term-accept.h"
#include "hev-fsh-client-port-accept.h"

#include "hev-fsh-client-forward.h"

static int
hev_fsh_client_forward_write_login (HevFshClientForward *self)
{
    const char *token = hev_fsh_config_get_token (self->base.config);
    HevFshMessage msg;
    int res;

    LOG_D ("%p fsh client forward write login", self);

    msg.ver = token ? 2 : 1;
    msg.cmd = HEV_FSH_CMD_LOGIN;
    res = hev_task_io_socket_send (self->base.fd, &msg, sizeof (msg),
                                   MSG_WAITALL, io_yielder, self);
    if (res <= 0)
        return -1;

    if (token) {
        HevFshMessageToken mtoken;

        res = hev_fsh_protocol_token_from_string (self->token, token);
        if (res < 0) {
            LOG_E ("%p fsh client forward token", self);
            return -1;
        }

        memcpy (mtoken.token, self->token, sizeof (HevFshToken));

        res = hev_task_io_socket_send (self->base.fd, &mtoken, sizeof (mtoken),
                                       MSG_WAITALL, io_yielder, self);
        if (res <= 0)
            return -1;
    }

    return 0;
}

static int
hev_fsh_client_forward_read_token (HevFshClientForward *self)
{
    HevFshMessageToken token;
    HevFshMessage msg;
    const char *src;
    char buf[40];
    int res;

    LOG_D ("%p fsh client forward read token", self);

    res = hev_task_io_socket_recv (self->base.fd, &msg, sizeof (msg),
                                   MSG_WAITALL, io_yielder, self);
    if (res <= 0)
        return -1;

    if (msg.cmd != HEV_FSH_CMD_TOKEN) {
        LOG_E ("%p fsh client forward login", self);
        return -1;
    }

    res = hev_task_io_socket_recv (self->base.fd, &token, sizeof (token),
                                   MSG_WAITALL, io_yielder, self);
    if (res <= 0)
        return -1;

    hev_fsh_protocol_token_to_string (token.token, buf);
    if (memcmp (token.token, self->token, sizeof (HevFshToken)) == 0) {
        src = "client";
    } else {
        src = "server";
        memcpy (self->token, token.token, sizeof (HevFshToken));
    }

    if (LOG_ON_D ())
        LOG_D ("%p fsh client forward token %s (from %s)", self, buf, src);
    else
        LOG_I ("token %s (from %s)", buf, src);

    return 0;
}

static int
hev_fsh_client_forward_write_keep_alive (HevFshClientForward *self)
{
    HevFshMessage msg;

    LOG_D ("%p fsh client forward keep alive", self);

    msg.ver = 2;
    msg.cmd = HEV_FSH_CMD_KEEP_ALIVE;

    return hev_task_io_socket_send (self->base.fd, &msg, sizeof (msg),
                                    MSG_WAITALL, io_yielder, self);
}

static void
hev_fsh_client_forward_task_entry (void *data)
{
    HevFshClientForward *self = data;
    HevFshClientBase *base = data;
    int keep_alive = 0;
    int res;

    res = hev_fsh_client_base_connect (base);
    if (res < 0) {
        LOG_E ("%p fsh client forward connect", self);
        goto restart;
    }

    res = hev_fsh_client_forward_write_login (self);
    if (res < 0)
        goto restart;

    res = hev_fsh_client_forward_read_token (self);
    if (res < 0)
        goto restart;

    for (;;) {
        HevFshClientBase *accept;
        HevFshMessageToken token;
        HevFshMessage msg;
        int mode;
        int res;

        res = hev_task_io_socket_recv (base->fd, &msg, sizeof (msg),
                                       MSG_WAITALL, io_yielder, self);
        if (res == -2) {
            if (keep_alive++)
                goto restart;
            res = hev_fsh_client_forward_write_keep_alive (self);
            if (res < 0)
                goto restart;
            continue;
        } else if (res <= 0) {
            goto restart;
        }

        switch (msg.cmd) {
        case HEV_FSH_CMD_CONNECT:
            break;
        case HEV_FSH_CMD_KEEP_ALIVE:
            keep_alive = 0;
            continue;
        default:
            goto restart;
        }

        res = hev_task_io_socket_recv (base->fd, &token, sizeof (token),
                                       MSG_WAITALL, io_yielder, self);
        if (res <= 0)
            goto restart;

        res = memcmp (token.token, self->token, sizeof (HevFshToken));
        if (res != 0)
            goto restart;

        mode = hev_fsh_config_get_mode (base->config);
        if (HEV_FSH_CONFIG_MODE_FORWARDER_PORT == mode)
            accept = hev_fsh_client_port_accept_new (base->config, self->token);
        else
            accept = hev_fsh_client_term_accept_new (base->config, self->token);

        if (accept)
            hev_fsh_io_run (HEV_FSH_IO (accept));

        res = hev_fsh_client_forward_write_keep_alive (self);
        if (res < 0)
            goto restart;
    }

restart:
    hev_object_unref (HEV_OBJECT (self));

    hev_task_sleep (1000);
    base = hev_fsh_client_forward_new (base->config);
    hev_fsh_io_run (HEV_FSH_IO (base));
}

static void
hev_fsh_client_forward_run (HevFshIO *base)
{
    LOG_D ("%p fsh client forward run", base);

    hev_task_run (base->task, hev_fsh_client_forward_task_entry, base);
}

HevFshClientBase *
hev_fsh_client_forward_new (HevFshConfig *config)
{
    HevFshClientForward *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClientForward));
    if (!self)
        return NULL;

    res = hev_fsh_client_forward_construct (self, config);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client forward new", self);

    return HEV_FSH_CLIENT_BASE (self);
}

int
hev_fsh_client_forward_construct (HevFshClientForward *self,
                                  HevFshConfig *config)
{
    int res;

    res = hev_fsh_client_base_construct (&self->base, config);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client forward construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_FORWARD_TYPE;

    return 0;
}

static void
hev_fsh_client_forward_destruct (HevObject *base)
{
    HevFshClientForward *self = HEV_FSH_CLIENT_FORWARD (base);

    LOG_D ("%p fsh client forward destruct", self);

    HEV_FSH_CLIENT_BASE_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_client_forward_class (void)
{
    static HevFshClientForwardClass klass;
    HevFshClientForwardClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshIOClass *ikptr;

        memcpy (kptr, HEV_FSH_CLIENT_BASE_TYPE, sizeof (HevFshClientBaseClass));

        okptr->name = "HevFshClientForward";
        okptr->finalizer = hev_fsh_client_forward_destruct;

        ikptr = HEV_FSH_IO_CLASS (kptr);
        ikptr->run = hev_fsh_client_forward_run;
    }

    return okptr;
}
