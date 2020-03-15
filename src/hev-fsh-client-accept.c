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

    if (hev_fsh_client_base_construct (&self->base, config, sm) < 0) {
        fprintf (stderr, "Construct client base failed!\n");
        goto exit;
    }

    s->task = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!s->task) {
        fprintf (stderr, "Create client accept's task failed!\n");
        goto exit_free;
    }

    memcpy (self->token, token, sizeof (HevFshToken));
    self->base._destroy = hev_fsh_client_accept_destroy;
    return 0;

exit_free:
    hev_fsh_client_base_destroy (&self->base);
exit:
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
    HevFshMessage msg;
    HevFshMessageToken msg_token;
    struct sockaddr_in saddr;
    struct sockaddr *addr;
    struct iovec iov[2];
    struct msghdr mh;
    int fd;

    fd = self->base.fd;
    hev_task_add_fd (hev_task_self (), fd, POLLIN | POLLOUT);

    addr = (struct sockaddr *)&saddr;
    if (hev_fsh_client_base_resolv (&self->base, addr) < 0)
        return -1;

    if (hev_task_io_socket_connect (fd, addr, sizeof (saddr),
                                    fsh_task_io_yielder, self) < 0)
        return -1;

    msg.ver = 1;
    msg.cmd = HEV_FSH_CMD_ACCEPT;
    memcpy (msg_token.token, self->token, sizeof (HevFshToken));

    iov[0].iov_base = &msg;
    iov[0].iov_len = sizeof (msg);
    iov[1].iov_base = &msg_token;
    iov[1].iov_len = sizeof (msg_token);

    memset (&mh, 0, sizeof (mh));
    mh.msg_iov = iov;
    mh.msg_iovlen = 2;

    if (hev_task_io_socket_sendmsg (fd, &mh, MSG_WAITALL, fsh_task_io_yielder,
                                    self) <= 0)
        return -1;

    return 0;
}
