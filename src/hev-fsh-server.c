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

#include "hev-fsh-server.h"

#define TIMEOUT (30 * 1000)

struct _HevFshServer
{
    int fd;
    int event_fds[2];
    int quit;

    HevTask *task_worker;
    HevTask *task_event;
    HevTask *task_session_manager;
    HevFshServerSessionBase *session_list;
};

static void hev_fsh_server_task_entry (void *data);
static void hev_fsh_server_event_task_entry (void *data);
static void hev_fsh_server_session_manager_task_entry (void *data);

static void session_manager_insert_session (HevFshServer *self,
                                            HevFshServerSession *session);
static void session_manager_remove_session (HevFshServer *self,
                                            HevFshServerSession *session);
static void session_close_handler (HevFshServerSession *session, void *data);

HevFshServer *
hev_fsh_server_new (HevFshConfig *config)
{
    HevFshServer *self;
    int fd, ret, reuseaddr = 1;
    const char *address;
    unsigned int port;
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

    self->task_worker = hev_task_new (8192);
    if (!self->task_worker) {
        fprintf (stderr, "Create server's task failed!\n");
        goto exit_free;
    }

    self->task_event = hev_task_new (8192);
    if (!self->task_event) {
        fprintf (stderr, "Create event's task failed!\n");
        goto exit_free_task_worker;
    }

    self->task_session_manager = hev_task_new (8192);
    if (!self->task_session_manager) {
        fprintf (stderr, "Create session manager's task failed!\n");
        goto exit_free_task_event;
    }

    return self;

exit_free_task_event:
    hev_task_unref (self->task_event);
exit_free_task_worker:
    hev_task_unref (self->task_worker);
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
    hev_free (self);
}

void
hev_fsh_server_start (HevFshServer *self)
{
    hev_task_run (self->task_worker, hev_fsh_server_task_entry, self);
    hev_task_run (self->task_event, hev_fsh_server_event_task_entry, self);
    hev_task_run (self->task_session_manager,
                  hev_fsh_server_session_manager_task_entry, self);
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

    return (self->quit) ? -1 : 0;
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
        HevFshServerSession *session;

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

        session =
            hev_fsh_server_session_new (client_fd, session_close_handler, self);
        if (!session) {
            close (client_fd);
            continue;
        }

        session_manager_insert_session (self, session);
        hev_fsh_server_session_run (session);
    }
}

static void
hev_fsh_server_event_task_entry (void *data)
{
    HevFshServer *self = data;
    HevTask *task = hev_task_self ();
    HevFshServerSessionBase *session;
    int val;

    if (-1 == hev_task_io_pipe_pipe (self->event_fds)) {
        fprintf (stderr, "Create eventfd failed!\n");
        return;
    }

    hev_task_add_fd (task, self->event_fds[0], POLLIN);
    hev_task_io_read (self->event_fds[0], &val, sizeof (val), NULL, NULL);

    /* set quit flag */
    self->quit = 1;
    /* wakeup worker's task */
    hev_task_wakeup (self->task_worker);
    /* wakeup session manager's task */
    hev_task_wakeup (self->task_session_manager);

    /* wakeup sessions's task */
#ifdef _DEBUG
    printf ("Enumerating session list ...\n");
#endif
    for (session = self->session_list; session; session = session->next) {
#ifdef _DEBUG
        printf ("Set session %p's hp = 0\n", session);
#endif
        session->hp = 0;

        /* wakeup session's task to do destroy */
        hev_task_wakeup (session->task);
#ifdef _DEBUG
        printf ("Wakeup session %p's task %p\n", session, session->task);
#endif
    }

    close (self->event_fds[0]);
    close (self->event_fds[1]);
}

static void
hev_fsh_server_session_manager_task_entry (void *data)
{
    HevFshServer *self = data;

    for (;;) {
        HevFshServerSessionBase *session;

        hev_task_sleep (TIMEOUT);
        if (self->quit)
            break;

#ifdef _DEBUG
        printf ("Enumerating session list ...\n");
#endif
        for (session = self->session_list; session; session = session->next) {
#ifdef _DEBUG
            printf ("Session %p's hp %d\n", session, session->hp);
#endif
            session->hp--;
            if (session->hp > 0)
                continue;

            /* wakeup session's task to do destroy */
            hev_task_wakeup (session->task);
#ifdef _DEBUG
            printf ("Wakeup session %p's task %p\n", session, session->task);
#endif
        }
    }
}

static void
session_manager_insert_session (HevFshServer *self,
                                HevFshServerSession *session)
{
    HevFshServerSessionBase *session_base = (HevFshServerSessionBase *)session;

#ifdef _DEBUG
    printf ("Insert session: %p\n", session);
#endif
    /* insert session to session_list */
    session_base->prev = NULL;
    session_base->next = self->session_list;
    if (self->session_list)
        self->session_list->prev = session_base;
    self->session_list = session_base;
}

static void
session_manager_remove_session (HevFshServer *self,
                                HevFshServerSession *session)
{
    HevFshServerSessionBase *session_base = (HevFshServerSessionBase *)session;

#ifdef _DEBUG
    printf ("Remove session: %p\n", session);
#endif
    /* remove session from session_list */
    if (session_base->prev) {
        session_base->prev->next = session_base->next;
    } else {
        self->session_list = session_base->next;
    }
    if (session_base->next) {
        session_base->next->prev = session_base->prev;
    }
}

static void
session_close_handler (HevFshServerSession *session, void *data)
{
    HevFshServer *self = data;

    session_manager_remove_session (self, session);
}
