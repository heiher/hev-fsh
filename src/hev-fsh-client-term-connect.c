/*
 ============================================================================
 Name        : hev-fsh-client-term-connect.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client term connect
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <termios.h>

#include "hev-fsh-client-term-connect.h"
#include "hev-fsh-protocol.h"
#include "hev-memory-allocator.h"
#include "hev-task.h"
#include "hev-task-io-socket.h"

#define TASK_STACK_SIZE (64 * 4096)

struct _HevFshClientTermConnect
{
    HevFshClientBase base;

    HevTask *task;
    HevFshConfig *config;
};

static void hev_fsh_client_term_connect_task_entry (void *data);
static void hev_fsh_client_term_connect_destroy (HevFshClientBase *self);

HevFshClientBase *
hev_fsh_client_term_connect_new (HevFshConfig *config)
{
    HevFshClientTermConnect *self;
    const char *address;
    unsigned int port;

    self = hev_malloc0 (sizeof (HevFshClientTermConnect));
    if (!self) {
        fprintf (stderr, "Allocate client term connect failed!\n");
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
        fprintf (stderr, "Create client term's task failed!\n");
        hev_free (self);
        return NULL;
    }

    self->config = config;
    self->base._destroy = hev_fsh_client_term_connect_destroy;

    hev_task_run (self->task, hev_fsh_client_term_connect_task_entry, self);

    return &self->base;
}

static void
hev_fsh_client_term_connect_destroy (HevFshClientBase *self)
{
    hev_free (self);
}

static void
hev_fsh_client_term_connect_task_entry (void *data)
{
    HevTask *task = hev_task_self ();
    HevFshClientTermConnect *self = data;
    HevFshMessage message;
    HevFshMessageToken message_token;
    HevFshMessageTermInfo message_term_info;
    struct termios term, term_rsh;
    struct winsize win_size;
    const char *token;
    ssize_t len;

    hev_task_add_fd (task, self->base.fd, EPOLLIN | EPOLLOUT);

    if (hev_task_io_socket_connect (self->base.fd, &self->base.address,
                                    sizeof (struct sockaddr_in), NULL,
                                    NULL) < 0) {
        fprintf (stderr, "Connect to server failed!\n");
        return;
    }

    message.ver = 1;
    message.cmd = HEV_FSH_CMD_CONNECT;

    len = hev_task_io_socket_send (self->base.fd, &message, sizeof (message),
                                   MSG_WAITALL, NULL, NULL);
    if (len <= 0)
        return;

    token = hev_fsh_config_get_token (self->config);
    if (hev_fsh_protocol_token_from_string (message_token.token, token) == -1) {
        fprintf (stderr, "Can't parse token!\n");
        return;
    }

    /* send message token */
    len = hev_task_io_socket_send (self->base.fd, &message_token,
                                   sizeof (message_token), MSG_WAITALL, NULL,
                                   NULL);
    if (len <= 0)
        return;

    if (ioctl (0, TIOCGWINSZ, &win_size) < 0)
        return;

    message_term_info.rows = win_size.ws_row;
    message_term_info.columns = win_size.ws_col;

    /* send message term info */
    len = hev_task_io_socket_send (self->base.fd, &message_term_info,
                                   sizeof (message_term_info), MSG_WAITALL,
                                   NULL, NULL);
    if (len <= 0)
        return;

    if (fcntl (0, F_SETFL, O_NONBLOCK) == -1)
        return;
    if (fcntl (1, F_SETFL, O_NONBLOCK) == -1)
        return;

    hev_task_add_fd (task, 0, EPOLLIN);
    hev_task_add_fd (task, 1, EPOLLOUT);

    if (tcgetattr (0, &term) == -1)
        return;

    memcpy (&term_rsh, &term, sizeof (term));
    term_rsh.c_oflag &= ~(OPOST);
    term_rsh.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK);
    if (tcsetattr (0, TCSADRAIN, &term_rsh) == -1)
        return;

    hev_task_io_splice (self->base.fd, self->base.fd, 0, 1, 2048, NULL, NULL);

    tcsetattr (0, TCSADRAIN, &term);
}
