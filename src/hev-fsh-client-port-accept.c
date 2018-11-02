/*
 ============================================================================
 Name        : hev-fsh-client-port-accept.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client port accept
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "hev-fsh-client-port-accept.h"
#include "hev-memory-allocator.h"
#include "hev-task.h"
#include "hev-task-io-socket.h"

#define TASK_STACK_SIZE (64 * 4096)

struct _HevFshClientPortAccept
{
    HevFshClientBase base;

    HevFshToken token;

    HevTask *task;
    HevFshConfig *config;
};

static void hev_fsh_client_port_accept_task_entry (void *data);
static void hev_fsh_client_port_accept_destroy (HevFshClientBase *self);

HevFshClientPortAccept *
hev_fsh_client_port_accept_new (HevFshConfig *config, HevFshToken token)
{
    HevFshClientPortAccept *self;
    const char *address;
    unsigned int port;

    self = hev_malloc0 (sizeof (HevFshClientPortAccept));
    if (!self) {
        fprintf (stderr, "Allocate client port accept failed!\n");
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
    memcpy (self->token, token, sizeof (HevFshToken));
    self->base._destroy = hev_fsh_client_port_accept_destroy;

    hev_task_run (self->task, hev_fsh_client_port_accept_task_entry, self);

    return self;
}

static void
hev_fsh_client_port_accept_destroy (HevFshClientBase *self)
{
    hev_free (self);
}

static void
hev_fsh_client_port_accept_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientPortAccept *self = data;
    HevFshMessage message;
    HevFshMessageToken message_token;
    HevFshMessagePortInfo message_port_info;
    int rfd, lfd;
    struct msghdr mh;
    struct iovec iov[2];
    struct sockaddr_in addr;

    rfd = self->base.fd;
    hev_task_add_fd (task, rfd, EPOLLIN | EPOLLOUT);

    if (hev_task_io_socket_connect (rfd, &self->base.address,
                                    sizeof (struct sockaddr_in), NULL,
                                    NULL) < 0)
        goto quit;

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

    if (hev_task_io_socket_sendmsg (rfd, &mh, MSG_WAITALL, NULL, NULL) <= 0)
        goto quit;

    /* recv message port info */
    if (0 > hev_task_io_socket_recv (rfd, &message_port_info,
                                     sizeof (message_port_info), MSG_WAITALL,
                                     NULL, NULL))
        goto quit;

    if (!hev_fsh_config_addr_list_contains (
            self->config, message_port_info.type, message_port_info.addr,
            message_port_info.port))
        goto quit;

    switch (message_port_info.type) {
    case 4:
        addr.sin_family = AF_INET;
        memcpy (&addr.sin_addr, message_port_info.addr, 4);
        break;
    default:
        goto quit;
    }
    addr.sin_port = message_port_info.port;

    lfd = socket (AF_INET, SOCK_STREAM, 0);
    if (0 > lfd)
        goto quit;

    if (fcntl (lfd, F_SETFL, O_NONBLOCK) == -1)
        goto quit_close_fd;

    hev_task_add_fd (task, lfd, EPOLLIN | EPOLLOUT);

    if (0 > hev_task_io_socket_connect (lfd, (struct sockaddr *)&addr,
                                        sizeof (addr), NULL, NULL))
        goto quit_close_fd;

    hev_task_io_splice (rfd, rfd, lfd, lfd, 2048, NULL, NULL);

quit_close_fd:
    close (lfd);
quit:
    hev_fsh_client_base_destroy ((HevFshClientBase *)self);
}
