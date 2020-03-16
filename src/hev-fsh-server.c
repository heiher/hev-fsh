/*
 ============================================================================
 Name        : hev-fsh-server.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2020 everyone.
 Description : Fsh server
 ============================================================================
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-pipe.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-main.h"
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
static void fsh_session_close_handler (HevFshSession *s, void *data);

HevFshBase *
hev_fsh_server_new (HevFshConfig *config)
{
    HevFshServer *self;
    struct sockaddr_in6 saddr;
    int fd, port, reuse = 1;
    unsigned int timeout;
    const char *addr;

    timeout = hev_fsh_config_get_timeout (config);
    addr = hev_fsh_config_get_server_address (config);
    port = hev_fsh_config_get_server_port (config);

    if (hev_fsh_parse_sockaddr (&saddr, addr, port) == 0) {
        fprintf (stderr, "Parse server address failed!\n");
        goto exit;
    }

    fd = hev_task_io_socket_socket (AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        fprintf (stderr, "Create socket failed!\n");
        goto exit;
    }

    if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (int)) < 0) {
        fprintf (stderr, "Set reuse address failed!\n");
        goto exit_close;
    }

    if (bind (fd, (struct sockaddr *)&saddr, sizeof (saddr)) < 0) {
        fprintf (stderr, "Bind address failed!\n");
        goto exit_close;
    }

    if (listen (fd, 10) < 0) {
        fprintf (stderr, "Listen failed!\n");
        goto exit_close;
    }

    self = hev_malloc0 (sizeof (HevFshServer));
    if (!self) {
        fprintf (stderr, "Allocate server failed!\n");
        goto exit_close;
    }

    self->session_manager = hev_fsh_session_manager_new (timeout, 0);
    if (!self->session_manager) {
        fprintf (stderr, "Create session manager failed!\n");
        goto exit_free;
    }

    self->task_event = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!self->task_event) {
        fprintf (stderr, "Create event's task failed!\n");
        goto exit_free_session_manager;
    }

    self->task_worker = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!self->task_worker) {
        fprintf (stderr, "Create server's task failed!\n");
        goto exit_free_task_event;
    }

    self->fd = fd;
    self->event_fds[0] = -1;
    self->event_fds[1] = -1;
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
    HevFshServer *self = HEV_FSH_SERVER (base);

    hev_task_unref (self->task_event);
    hev_task_unref (self->task_worker);
    hev_fsh_session_manager_destroy (self->session_manager);

    close (self->fd);
    hev_free (self);
}

static void
hev_fsh_server_start (HevFshBase *base)
{
    HevFshServer *self = HEV_FSH_SERVER (base);

    hev_fsh_session_manager_start (self->session_manager);

    hev_task_ref (self->task_event);
    hev_task_run (self->task_event, hev_fsh_server_event_task_entry, self);

    hev_task_ref (self->task_worker);
    hev_task_run (self->task_worker, hev_fsh_server_worker_task_entry, self);
}

static void
hev_fsh_server_stop (HevFshBase *base)
{
    HevFshServer *self = HEV_FSH_SERVER (base);
    int val;

    if (self->event_fds[1] < 0)
        return;

    if (write (self->event_fds[1], &val, sizeof (val)) < 0)
        fprintf (stderr, "Write stop event failed!\n");
}

static int
fsh_task_io_yielder (HevTaskYieldType type, void *data)
{
    HevFshServer *self = HEV_FSH_SERVER (data);

    hev_task_yield (type);

    return (self->quit) ? -1 : 0;
}

static void
hev_fsh_server_event_task_entry (void *data)
{
    HevFshServer *self = HEV_FSH_SERVER (data);
    int val;

    if (hev_task_io_pipe_pipe (self->event_fds) < 0) {
        fprintf (stderr, "Create eventfd failed!\n");
        return;
    }

    hev_task_add_fd (hev_task_self (), self->event_fds[0], POLLIN);
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
    HevFshServer *self = HEV_FSH_SERVER (data);

    hev_task_add_fd (hev_task_self (), self->fd, POLLIN);

    for (;;) {
        HevFshServerSession *ss;
        HevFshSession *s;
        int fd;

        fd = hev_task_io_socket_accept (self->fd, NULL, NULL,
                                        fsh_task_io_yielder, self);
        if (fd == -2) {
            break;
        } else if (fd < 0) {
            fprintf (stderr, "Accept failed!\n");
            continue;
        }

        ss = hev_fsh_server_session_new (fd, fsh_session_close_handler, self);
        if (!ss) {
            close (fd);
            continue;
        }

        s = HEV_FSH_SESSION (ss);
        hev_fsh_session_manager_insert (self->session_manager, s);
        hev_fsh_server_session_run (ss);
    }
}

static void
fsh_session_close_handler (HevFshSession *s, void *data)
{
    HevFshServer *self = HEV_FSH_SERVER (data);

    hev_fsh_session_manager_remove (self->session_manager, s);
}
