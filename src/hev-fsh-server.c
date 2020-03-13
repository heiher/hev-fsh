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

#include "hev-task.h"
#include "hev-task-io.h"
#include "hev-task-io-pipe.h"
#include "hev-task-io-socket.h"
#include "hev-memory-allocator.h"

#include "hev-fsh-server-session.h"
#include "hev-fsh-session-manager.h"

#include "hev-fsh-server.h"

struct _HevFshServer
{
    int fd;
    int event_fds[2];

    HevTask *task_worker;
    HevTask *task_event;
    HevFshSessionManager *session_manager;
};

static void hev_fsh_server_task_entry (void *data);
static void hev_fsh_server_event_task_entry (void *data);
static void session_close_handler (HevFshSession *s, void *data);

HevFshServer *
hev_fsh_server_new (HevFshConfig *config)
{
    HevFshServer *self;
    int fd, ret, reuseaddr = 1;
    const char *address;
    unsigned int port;
    unsigned int timeout;
    struct sockaddr_in addr;

    fd = hev_task_io_socket_socket (AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        fprintf (stderr, "Create socket failed!\n");
        goto exit;
    }

    ret = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                      sizeof (reuseaddr));
    if (ret == -1) {
        fprintf (stderr, "Set reuse address failed!\n");
        goto exit_close;
    }

    address = hev_fsh_config_get_server_address (config);
    port = hev_fsh_config_get_server_port (config);

    memset (&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr (address);
    addr.sin_port = htons (port);
    ret = bind (fd, (struct sockaddr *)&addr, (socklen_t)sizeof (addr));
    if (ret == -1) {
        fprintf (stderr, "Bind address failed!\n");
        goto exit_close;
    }
    ret = listen (fd, 100);
    if (ret == -1) {
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

    self->task_worker = hev_task_new (8192);
    if (!self->task_worker) {
        fprintf (stderr, "Create server's task failed!\n");
        goto exit_free_session_manager;
    }

    self->task_event = hev_task_new (8192);
    if (!self->task_event) {
        fprintf (stderr, "Create event's task failed!\n");
        goto exit_free_task_worker;
    }

    return self;

exit_free_task_worker:
    hev_task_unref (self->task_worker);
exit_free_session_manager:
    hev_fsh_session_manager_destroy (self->session_manager);
exit_free:
    hev_free (self);
exit_close:
    close (fd);
exit:
    return NULL;
}

void
hev_fsh_server_destroy (HevFshServer *self)
{
    close (self->fd);
    hev_fsh_session_manager_destroy (self->session_manager);
    hev_free (self);
}

void
hev_fsh_server_start (HevFshServer *self)
{
    hev_fsh_session_manager_start (self->session_manager);
    hev_task_run (self->task_worker, hev_fsh_server_task_entry, self);
    hev_task_run (self->task_event, hev_fsh_server_event_task_entry, self);
}

void
hev_fsh_server_stop (HevFshServer *self)
{
    int val;

    if (self->event_fds[1] == -1)
        return;

    if (write (self->event_fds[1], &val, sizeof (val)) == -1)
        fprintf (stderr, "Write stop event failed!\n");
}

static int
fsh_task_io_yielder (HevTaskYieldType type, void *data)
{
    HevFshServer *self = data;

    hev_task_yield (type);

    return (self->task_worker) ? 0 : -1;
}

static void
hev_fsh_server_task_entry (void *data)
{
    HevFshServer *self = data;
    HevTask *task = hev_task_self ();

    hev_task_add_fd (task, self->fd, POLLIN);

    for (;;) {
        int client_fd;
        struct sockaddr_in addr;
        struct sockaddr *in_addr = (struct sockaddr *)&addr;
        socklen_t addr_len = sizeof (addr);
        HevFshServerSession *ss;
        HevFshSession *s;

        client_fd = hev_task_io_socket_accept (self->fd, in_addr, &addr_len,
                                               fsh_task_io_yielder, self);
        if (client_fd < 0) {
            fprintf (stderr, "Accept failed!\n");
            continue;
        } else if (-2 == client_fd) {
            break;
        }

#ifdef _DEBUG
        printf ("New client %d enter from %s:%u\n", client_fd,
                inet_ntoa (addr.sin_addr), ntohs (addr.sin_port));
#endif

        ss =
            hev_fsh_server_session_new (client_fd, session_close_handler, self);
        if (!ss) {
            close (client_fd);
            continue;
        }

        s = (HevFshSession *)ss;
        hev_fsh_session_manager_insert (self->session_manager, s);
        hev_fsh_server_session_run (ss);
    }
}

static void
hev_fsh_server_event_task_entry (void *data)
{
    HevFshServer *self = data;
    HevTask *task = hev_task_self ();
    int val;

    if (-1 == hev_task_io_pipe_pipe (self->event_fds)) {
        fprintf (stderr, "Create eventfd failed!\n");
        return;
    }

    hev_task_add_fd (task, self->event_fds[0], POLLIN);
    hev_task_io_read (self->event_fds[0], &val, sizeof (val), NULL, NULL);

    hev_task_wakeup (self->task_worker);
    self->task_worker = NULL;
    hev_fsh_session_manager_stop (self->session_manager);

    close (self->event_fds[0]);
    close (self->event_fds[1]);
}

static void
session_close_handler (HevFshSession *s, void *data)
{
    HevFshServer *self = data;

    hev_fsh_session_manager_remove (self->session_manager, s);
}
