/*
 ============================================================================
 Name        : hev-fsh-client-term-connect.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client term connect
 ============================================================================
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-fsh-protocol.h"

#include "hev-fsh-client-term-connect.h"

#define fsh_task_io_yielder hev_fsh_session_task_io_yielder

struct _HevFshClientTermConnect
{
    HevFshClientConnect base;
};

static void hev_fsh_client_term_connect_task_entry (void *data);
static void hev_fsh_client_term_connect_destroy (HevFshClientConnect *base);

HevFshClientBase *
hev_fsh_client_term_connect_new (HevFshConfig *config, HevFshSessionManager *sm)
{
    HevFshClientTermConnect *self;
    HevFshSession *s;

    self = hev_malloc0 (sizeof (HevFshClientTermConnect));
    if (!self) {
        fprintf (stderr, "Allocate client term connect failed!\n");
        goto exit;
    }

    if (hev_fsh_client_connect_construct (&self->base, config, sm) < 0) {
        fprintf (stderr, "Construct client connect failed!\n");
        goto exit_free;
    }

    s = HEV_FSH_SESSION (self);
    hev_task_run (s->task, hev_fsh_client_term_connect_task_entry, self);

    self->base._destroy = hev_fsh_client_term_connect_destroy;
    return &self->base.base;

exit_free:
    hev_free (self);
exit:
    return NULL;
}

static void
hev_fsh_client_term_connect_destroy (HevFshClientConnect *base)
{
    hev_free (base);
}

static void
hev_fsh_client_term_connect_task_entry (void *data)
{
    HevFshClientTermConnect *self = HEV_FSH_CLIENT_TERM_CONNECT (data);
    HevFshMessageTermInfo msg_tinfo;
    struct termios term, term_rsh;
    struct winsize win_size;

    if (hev_fsh_client_connect_send_connect (&self->base) < 0)
        goto exit;

    if (ioctl (0, TIOCGWINSZ, &win_size) < 0)
        goto exit;

    msg_tinfo.rows = win_size.ws_row;
    msg_tinfo.columns = win_size.ws_col;

    /* send message term info */
    if (hev_task_io_socket_send (self->base.base.fd, &msg_tinfo,
                                 sizeof (msg_tinfo), MSG_WAITALL,
                                 fsh_task_io_yielder, self) <= 0)
        goto exit;

    if (fcntl (0, F_SETFL, O_NONBLOCK) < 0)
        goto exit;
    if (fcntl (1, F_SETFL, O_NONBLOCK) < 0)
        goto exit;

    hev_task_add_fd (hev_task_self (), 0, POLLIN);
    hev_task_add_fd (hev_task_self (), 1, POLLOUT);

    if (tcgetattr (0, &term) < 0)
        goto exit;

    memcpy (&term_rsh, &term, sizeof (term));
    term_rsh.c_oflag &= ~(OPOST);
    term_rsh.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK);
    if (tcsetattr (0, TCSADRAIN, &term_rsh) < 0)
        goto exit;

    hev_task_io_splice (self->base.base.fd, self->base.base.fd, 0, 1, 8192,
                        fsh_task_io_yielder, self);

    tcsetattr (0, TCSADRAIN, &term);

exit:
    hev_fsh_client_base_destroy (&self->base.base);
}
