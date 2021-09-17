/*
 ============================================================================
 Name        : hev-fsh-client-connect.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client connect
 ============================================================================
 */

#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>

#include "hev-logger.h"
#include "hev-fsh-protocol.h"

#include "hev-fsh-client-connect.h"

int
hev_fsh_client_connect_send_connect (HevFshClientConnect *self)
{
    HevFshClientBase *base = HEV_FSH_CLIENT_BASE (self);
    HevFshMessageToken msg_token;
    HevFshMessage msg;
    const char *token;
    int res;

    LOG_D ("%p fsh client connect send connect", self);

    res = hev_fsh_client_base_connect (base);
    if (res < 0) {
        LOG_E ("%p fsh client connect connect", self);
        return -1;
    }

    msg.ver = 1;
    msg.cmd = HEV_FSH_CMD_CONNECT;
    token = hev_fsh_config_get_token (base->config);

    res = hev_task_io_socket_send (base->fd, &msg, sizeof (msg), MSG_WAITALL,
                                   io_yielder, self);
    if (res <= 0)
        return -1;

    res = hev_fsh_protocol_token_from_string (msg_token.token, token);
    if (res == -1) {
        LOG_E ("%p fsh client connect token", self);
        return -1;
    }

    res = hev_task_io_socket_send (base->fd, &msg_token, sizeof (msg_token),
                                   MSG_WAITALL, io_yielder, self);
    if (res <= 0)
        return -1;

    return 0;
}

int
hev_fsh_client_connect_construct (HevFshClientConnect *self,
                                  HevFshConfig *config)
{
    int res;

    res = hev_fsh_client_base_construct (&self->base, config);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client connect construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_CONNECT_TYPE;

    return 0;
}

static void
hev_fsh_client_connect_destruct (HevObject *base)
{
    HevFshClientConnect *self = HEV_FSH_CLIENT_CONNECT (base);

    LOG_D ("%p fsh client connect destruct", self);

    HEV_FSH_CLIENT_BASE_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_client_connect_class (void)
{
    static HevFshClientConnectClass klass;
    HevFshClientConnectClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        memcpy (kptr, HEV_FSH_CLIENT_BASE_TYPE, sizeof (HevFshClientBaseClass));

        okptr->name = "HevFshClientConnect";
        okptr->finalizer = hev_fsh_client_connect_destruct;
    }

    return okptr;
}
