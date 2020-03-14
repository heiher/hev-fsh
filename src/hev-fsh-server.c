/*
 ============================================================================
 Name        : hev-fsh-server.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2019 everyone.
 Description : Fsh server
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-pipe.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-fsh-server-session.h"
#include "hev-fsh-session-manager.h"

#include "hev-fsh-server.h"

struct _HevFshServer
{
    HevFshBase base;

    int fd;
    int quit;
    int event_fds[2];

    HevTask *task_event;
    HevTask *task_worker;
    HevFshSessionManager *session_manager;
};

static void hev_fsh_server_destroy (HevFshBase *base);
static void hev_fsh_server_start (HevFshBase *base);
static void hev_fsh_server_stop (HevFshBase *base);
static void hev_fsh_server_event_task_entry (void *data);
static void hev_fsh_server_worker_task_entry (void *data);
static void session_close_handler (HevFshSession *s, void *data);

HevFshBase *
hev_fsh_server_new (HevFshConfig *config)
{
    HevFshServer *self;
    int fd, port, reuseaddr = 1;
    struct sockaddr_in address;
    unsigned int timeout;
    const char *addr;

    fd = hev_task_io_socket_socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf (stderr, "Create socket failed!\n");
        goto exit;
    }

    if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                    sizeof (reuseaddr)) < 0) {
        fprintf (stderr, "Set reuse address failed!\n");
        goto exit_close;
    }

    addr = hev_fsh_config_get_server_address (config);
    port = hev_fsh_config_get_server_port (config);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr (addr);
    address.sin_port = htons (port);

    if (bind (fd, (struct sockaddr *)&address, sizeof (address)) < 0) {
        fprintf (stderr, "Bind address failed!\n");
        goto exit_close;
    }

    if (listen (fd, 100) < 0) {
        fprintf (stderr, "Listen failed!\n");
        goto exit_close;
    }

    self = hev_malloc0 (sizeof (HevFshServer));
    if (!self) {
        fprintf (stderr, "Allocate server failed!\n");
        goto exit_close;
    }

    self->fd = fd;
    self->event_fds[0] = -1;
    self->event_fds[1] = -1;

    timeout = hev_fsh_config_get_timeout (config);
    self->session_manager = hev_fsh_session_manager_new (timeout, 0);
    if (!self->session_manager) {
        fprintf (stderr, "Create session manager failed!\n");
        goto exit_free;
    }

    self->task_event = hev_task_new (8192);
    if (!self->task_event) {
        fprintf (stderr, "Create event's task failed!\n");
        goto exit_free_session_manager;
    }

    self->task_worker = hev_task_new (8192);
    if (!self->task_worker) {
        fprintf (stderr, "Create server's task failed!\n");
        goto exit_free_task_event;
    }

    self->base._destroy = hev_fsh_server_destroy;
    self->base._start = hev_fsh_server_start;
    self->base._stop = hev_fsh_server_stop;

    return &self->base;

exit_free_task_event:
    hev_task_unref (self->task_event);
exit_free_session_manager:
    hev_fsh_session_manager_destroy (self->session_manager);
exit_free:
    hev_free (self);
exit_close:
    close (fd);
exit:
    return NULL;
}

static void
hev_fsh_server_destroy (HevFshBase *base)
{
    HevFshServer *self = (HevFshServer *)base;

    close (self->fd);
    hev_task_unref (self->task_event);
    hev_task_unref (self->task_worker);
    hev_fsh_session_manager_destroy (self->session_manager);
    hev_free (self);
}

static void
hev_fsh_server_start (HevFshBase *base)
{
    HevFshServer *self = (HevFshServer *)base;

    hev_fsh_session_manager_start (self->session_manager);
    hev_task_ref (self->task_event);
    hev_task_run (self->task_event, hev_fsh_server_event_task_entry, self);
    hev_task_ref (self->task_worker);
    hev_task_run (self->task_worker, hev_fsh_server_worker_task_entry, self);
}

static void
hev_fsh_server_stop (HevFshBase *base)
{
    HevFshServer *self = (HevFshServer *)base;
    int val;

    if (self->event_fds[1] < 0)
        return;

    if (write (self->event_fds[1], &val, sizeof (val)) < 0)
        fprintf (stderr, "Write stop event failed!\n");
}

static int
fsh_task_io_yielder (HevTaskYieldType type, void *data)
{
    HevFshServer *self = data;

    hev_task_yield (type);

    return (self->quit) ? -1 : 0;
}

static void
hev_fsh_server_event_task_entry (void *data)
{
    HevFshServer *self = data;
    HevTask *task = hev_task_self ();
    int val;

    if (hev_task_io_pipe_pipe (self->event_fds) < 0) {
        fprintf (stderr, "Create eventfd failed!\n");
        return;
    }

    hev_task_add_fd (task, self->event_fds[0], POLLIN);
    hev_task_io_read (self->event_fds[0], &val, sizeof (val), NULL, NULL);

    self->quit = 1;
    hev_task_wakeup (self->task_worker);
    hev_fsh_session_manager_stop (self->session_manager);

    close (self->event_fds[0]);
    close (self->event_fds[1]);
}

static void
hev_fsh_server_worker_task_entry (void *data)
{
    HevFshServer *self = data;
    HevTask *task = hev_task_self ();

    hev_task_add_fd (task, self->fd, POLLIN);

    for (;;) {
        int fd;
        HevFshServerSession *ss;
        HevFshSession *s;

        fd = hev_task_io_socket_accept (self->fd, NULL, NULL,
                                        fsh_task_io_yielder, self);
        if (-2 == fd) {
            break;
        } else if (fd < 0) {
            fprintf (stderr, "Accept failed!\n");
            continue;
        }

        ss = hev_fsh_server_session_new (fd, session_close_handler, self);
        if (!ss) {
            close (fd);
            continue;
        }

        s = (HevFshSession *)ss;
        hev_fsh_session_manager_insert (self->session_manager, s);
        hev_fsh_server_session_run (ss);
    }
}

static void
session_close_handler (HevFshSession *s, void *data)
{
    HevFshServer *self = data;

    hev_fsh_session_manager_remove (self->session_manager, s);
}
