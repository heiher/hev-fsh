/*
 ============================================================================
 Name        : hev-fsh-client-accept.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client accept
 ============================================================================
 */

#include <string.h>
#include <sys/types.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>

#include "hev-logger.h"

#include "hev-fsh-client-accept.h"

int
hev_fsh_client_accept_send_accept (HevFshClientAccept *self)
{
    HevFshClientBase *base = HEV_FSH_CLIENT_BASE (self);
    HevFshMessageToken msg_token;
    HevFshMessage msg;
    struct iovec iov[2];
    struct msghdr mh;
    int res;

    LOG_D ("%p fsh client accept send accept", self);

    res = hev_fsh_client_base_connect (base);
    if (res < 0)
        return -1;

    msg.ver = 1;
    msg.cmd = HEV_FSH_CMD_ACCEPT;
    memcpy (msg_token.token, self->token, sizeof (HevFshToken));

    iov[0].iov_base = &msg;
    iov[0].iov_len = sizeof (msg);
    iov[1].iov_base = &msg_token;
    iov[1].iov_len = sizeof (msg_token);

    __builtin_bzero (&mh, sizeof (mh));
    mh.msg_iov = iov;
    mh.msg_iovlen = 2;

    res = hev_task_io_socket_sendmsg (base->fd, &mh, MSG_WAITALL, io_yielder,
                                      self);
    if (res <= 0)
        return -1;

    return 0;
}

int
hev_fsh_client_accept_construct (HevFshClientAccept *self, HevFshConfig *config,
                                 HevFshToken token)
{
    int res;

    res = hev_fsh_client_base_construct (&self->base, config);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client accept construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_ACCEPT_TYPE;

    memcpy (self->token, token, sizeof (HevFshToken));

    return 0;
}

static void
hev_fsh_client_accept_destruct (HevObject *base)
{
    HevFshClientAccept *self = HEV_FSH_CLIENT_ACCEPT (base);

    LOG_D ("%p fsh client accept destruct", self);

    HEV_FSH_CLIENT_BASE_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_client_accept_class (void)
{
    static HevFshClientAcceptClass klass;
    HevFshClientAcceptClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        memcpy (kptr, HEV_FSH_CLIENT_BASE_TYPE, sizeof (HevFshClientBaseClass));

        okptr->name = "HevFshClientAccept";
        okptr->finalizer = hev_fsh_client_accept_destruct;
    }

    return okptr;
}
