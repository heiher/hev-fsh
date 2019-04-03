/*
 ============================================================================
 Name        : hev-fsh-client-accept.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client accept
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "hev-fsh-client-accept.h"
#include "hev-task-io.h"
#include "hev-task-io-socket.h"

#define TASK_STACK_SIZE (16384)

static void hev_fsh_client_accept_destroy (HevFshClientBase *base);

int
hev_fsh_client_accept_construct (HevFshClientAccept *self, HevFshConfig *config,
                                 HevFshToken token)
{
    const char *address;
    unsigned int port;

    address = hev_fsh_config_get_server_address (config);
    port = hev_fsh_config_get_server_port (config);

    if (0 > hev_fsh_client_base_construct (&self->base, address, port)) {
        fprintf (stderr, "Construct client base failed!\n");
        return -1;
    }

    self->task = hev_task_new (TASK_STACK_SIZE);
    if (!self->task) {
        fprintf (stderr, "Create client accept's task failed!\n");
        return -1;
    }

    self->config = config;
    memcpy (self->token, token, sizeof (HevFshToken));
    self->base._destroy = hev_fsh_client_accept_destroy;

    return 0;
}

static void
hev_fsh_client_accept_destroy (HevFshClientBase *base)
{
    HevFshClientAccept *self = (HevFshClientAccept *)base;

    if (self->_destroy)
        self->_destroy (self);
}

int
hev_fsh_client_accept_send_accept (HevFshClientAccept *self)
{
    HevTask *task = hev_task_self ();
    HevFshMessage message;
    HevFshMessageToken message_token;
    struct msghdr mh;
    struct iovec iov[2];
    int fd;

    fd = self->base.fd;
    hev_task_add_fd (task, fd, POLLIN | POLLOUT);

    if (hev_task_io_socket_connect (fd, &self->base.address,
                                    sizeof (struct sockaddr_in), NULL,
                                    NULL) < 0)
        return -1;

    message.ver = 1;
    message.cmd = HEV_FSH_CMD_ACCEPT;
    memcpy (message_token.token, self->token, sizeof (HevFshToken));

    memset (&mh, 0, sizeof (mh));
    mh.msg_iov = iov;
    mh.msg_iovlen = 2;

    iov[0].iov_base = &message;
    iov[0].iov_len = sizeof (message);
    iov[1].iov_base = &message_token;
    iov[1].iov_len = sizeof (message_token);

    if (hev_task_io_socket_sendmsg (fd, &mh, MSG_WAITALL, NULL, NULL) <= 0)
        return -1;

    return 0;
}
