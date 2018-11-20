/*
 ============================================================================
 Name        : hev-fsh-server.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 everyone.
 Description : Fsh server
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "hev-fsh-server.h"
#include "hev-fsh-server-session.h"
#include "hev-memory-allocator.h"
#include "hev-task.h"
#include "hev-task-io.h"
#include "hev-task-io-socket.h"

#define TIMEOUT (30 * 1000)

struct _HevFshServer
{
    int fd;
    int event_fd;
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
        return NULL;
    }

    ret = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                      sizeof (reuseaddr));
    if (ret == -1) {
        fprintf (stderr, "Set reuse address failed!\n");
        close (fd);
        return NULL;
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
        close (fd);
        return NULL;
    }
    ret = listen (fd, 100);
    if (ret == -1) {
        fprintf (stderr, "Listen failed!\n");
        close (fd);
        return NULL;
    }

    self = hev_malloc0 (sizeof (HevFshServer));
    if (!self) {
        fprintf (stderr, "Allocate server failed!\n");
        return NULL;
    }

    self->fd = fd;
    self->event_fd = -1;

    self->task_worker = hev_task_new (8192);
    if (!self->task_worker) {
        fprintf (stderr, "Create server's task failed!\n");
        hev_free (self);
        return NULL;
    }

    self->task_event = hev_task_new (8192);
    if (!self->task_event) {
        fprintf (stderr, "Create event's task failed!\n");
        hev_task_unref (self->task_worker);
        hev_free (self);
        return NULL;
    }

    self->task_session_manager = hev_task_new (8192);
    if (!self->task_session_manager) {
        fprintf (stderr, "Create session manager's task failed!\n");
        hev_task_unref (self->task_event);
        hev_task_unref (self->task_worker);
        hev_free (self);
        return NULL;
    }

    return self;
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
    if (self->event_fd == -1)
        return;

    if (eventfd_write (self->event_fd, 1) == -1)
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

    hev_task_add_fd (task, self->fd, EPOLLIN);

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
    ssize_t size;
    HevFshServerSessionBase *session;

    self->event_fd = eventfd (0, EFD_NONBLOCK);
    if (-1 == self->event_fd) {
        fprintf (stderr, "Create eventfd failed!\n");
        return;
    }

    hev_task_add_fd (task, self->event_fd, EPOLLIN);

    for (;;) {
        eventfd_t val;
        size = eventfd_read (self->event_fd, &val);
        if (-1 == size && errno == EAGAIN) {
            hev_task_yield (HEV_TASK_WAITIO);
            continue;
        }
        break;
    }

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

    close (self->event_fd);
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
