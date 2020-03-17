/*
 ============================================================================
 Name        : hev-fsh-client-accept.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client accept
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>

#include "hev-fsh-client-accept.h"

#define fsh_task_io_yielder hev_fsh_session_task_io_yielder

static void hev_fsh_client_accept_destroy (HevFshClientBase *base);

int
hev_fsh_client_accept_construct (HevFshClientAccept *self, HevFshConfig *config,
                                 HevFshToken token, HevFshSessionManager *sm)
{
    HevFshSession *s = HEV_FSH_SESSION (self);

    hev_fsh_client_base_init (&self->base, config, sm);

    s->task = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!s->task) {
        fprintf (stderr, "Create client accept's task failed!\n");
        goto exit;
    }

    memcpy (self->token, token, sizeof (HevFshToken));
    self->base._destroy = hev_fsh_client_accept_destroy;
    return 0;

exit:
    hev_fsh_client_base_destroy (&self->base);
    return -1;
}

static void
hev_fsh_client_accept_destroy (HevFshClientBase *base)
{
    HevFshClientAccept *self = HEV_FSH_CLIENT_ACCEPT (base);

    if (self->_destroy)
        self->_destroy (self);
}

int
hev_fsh_client_accept_send_accept (HevFshClientAccept *self)
{
    HevFshClientBase *base = HEV_FSH_CLIENT_BASE (self);
    HevFshMessageToken msg_token;
    HevFshMessage msg;
    struct iovec iov[2];
    struct msghdr mh;

    if (hev_fsh_client_base_connect (base) < 0)
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

    if (hev_task_io_socket_sendmsg (base->fd, &mh, MSG_WAITALL,
                                    fsh_task_io_yielder, self) <= 0)
        return -1;

    return 0;
}
