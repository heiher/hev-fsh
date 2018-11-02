/*
 ============================================================================
 Name        : hev-fsh-client-port-connect.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client port connect
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

#include "hev-fsh-client-port-connect.h"
#include "hev-fsh-protocol.h"
#include "hev-memory-allocator.h"
#include "hev-task.h"
#include "hev-task-io-socket.h"

#define TASK_STACK_SIZE (64 * 4096)

struct _HevFshClientPortConnect
{
    HevFshClientBase base;

    int local_fd;

    HevTask *task;
    HevFshConfig *config;
};

static void hev_fsh_client_port_connect_task_entry (void *data);
static void hev_fsh_client_port_connect_destroy (HevFshClientBase *base);

HevFshClientBase *
hev_fsh_client_port_connect_new (HevFshConfig *config, int local_fd)
{
    HevFshClientPortConnect *self;
    const char *address;
    unsigned int port;

    self = hev_malloc0 (sizeof (HevFshClientPortConnect));
    if (!self) {
        fprintf (stderr, "Allocate client port connect failed!\n");
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
    self->local_fd = local_fd;
    self->base._destroy = hev_fsh_client_port_connect_destroy;

    hev_task_run (self->task, hev_fsh_client_port_connect_task_entry, self);

    return &self->base;
}

static void
hev_fsh_client_port_connect_destroy (HevFshClientBase *base)
{
    HevFshClientPortConnect *self = (HevFshClientPortConnect *)base;

    close (self->local_fd);
    hev_free (self);
}

static void
hev_fsh_client_port_connect_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientPortConnect *self = data;
    HevFshMessage message;
    HevFshMessageToken message_token;
    HevFshMessagePortInfo message_port_info;
    const char *token;
    const char *address;
    unsigned int port;
    ssize_t len;

    hev_task_add_fd (task, self->base.fd, EPOLLIN | EPOLLOUT);

    if (hev_task_io_socket_connect (self->base.fd, &self->base.address,
                                    sizeof (struct sockaddr_in), NULL,
                                    NULL) < 0) {
        fprintf (stderr, "Connect to server failed!\n");
        goto quit;
    }

    message.ver = 1;
    message.cmd = HEV_FSH_CMD_CONNECT;

    len = hev_task_io_socket_send (self->base.fd, &message, sizeof (message),
                                   MSG_WAITALL, NULL, NULL);
    if (len <= 0)
        goto quit;

    token = hev_fsh_config_get_token (self->config);
    if (hev_fsh_protocol_token_from_string (message_token.token, token) == -1) {
        fprintf (stderr, "Can't parse token!\n");
        goto quit;
    }

    /* send message token */
    len = hev_task_io_socket_send (self->base.fd, &message_token,
                                   sizeof (message_token), MSG_WAITALL, NULL,
                                   NULL);
    if (len <= 0)
        goto quit;

    address = hev_fsh_config_get_remote_address (self->config);
    port = hev_fsh_config_get_remote_port (self->config);
    message_port_info.type = 4;
    message_port_info.port = htons (port);
    inet_aton (address, (struct in_addr *)&message_port_info.addr);

    /* send message port info */
    len = hev_task_io_socket_send (self->base.fd, &message_port_info,
                                   sizeof (message_port_info), MSG_WAITALL,
                                   NULL, NULL);
    if (len <= 0)
        goto quit;

    if (fcntl (self->local_fd, F_SETFL, O_NONBLOCK) == -1)
        goto quit;

    hev_task_add_fd (task, self->local_fd, EPOLLIN | EPOLLOUT);

    hev_task_io_splice (self->base.fd, self->base.fd, self->local_fd,
                        self->local_fd, 2048, NULL, NULL);

quit:
    hev_fsh_client_base_destroy ((HevFshClientBase *)self);
}
