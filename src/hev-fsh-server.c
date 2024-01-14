/*
 ============================================================================
 Name        : hev-fsh-server.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2023 xyz
 Description : Fsh server
 ============================================================================
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-pipe.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-fsh-session.h"

#include "hev-fsh-server.h"

static void
hev_fsh_server_task_entry (void *data)
{
    HevFshServer *self = HEV_FSH_SERVER (data);
    unsigned int timeout;

    timeout = hev_fsh_config_get_timeout (self->config);
    hev_task_add_fd (hev_task_self (), self->fd, POLLIN);

    for (;;) {
        HevFshSession *s;
        int fd;

        fd = hev_task_io_socket_accept (self->fd, NULL, NULL, NULL, NULL);
        if (fd < 0) {
            LOG_W ("%p fsh server accept", self);
            continue;
        }

        s = hev_fsh_session_new (fd, timeout, self->t_mgr, self->s_mgr);
        if (!s) {
            close (fd);
            continue;
        }

        hev_fsh_io_run (HEV_FSH_IO (s));
    }
}

static void
hev_fsh_event_task_entry (void *data)
{
}

HevFshBase *
hev_fsh_server_new (HevFshConfig *config)
{
    HevFshServer *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshServer));
    if (!self)
        return NULL;

    res = hev_fsh_server_construct (self, config);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh server new", self);

    return HEV_FSH_BASE (self);
}

void
hev_fsh_server_start (HevFshBase *base)
{
    HevFshServer *self = HEV_FSH_SERVER (base);

    LOG_D ("%p fsh server start", base);

    hev_task_ref (self->main_task);
    hev_task_run (self->main_task, hev_fsh_server_task_entry, self);

    if (self->event_task) {
        hev_task_ref (self->event_task);
        hev_task_run (self->event_task, hev_fsh_event_task_entry, self);
    }
}

void
hev_fsh_server_stop (HevFshBase *base)
{
    LOG_D ("%p fsh clinet stop", base);

    exit (0);
}

void
hev_fsh_server_reload (HevFshBase *base)
{
    HevFshServer *self = HEV_FSH_SERVER (base);

    LOG_D ("%p fsh clinet reload", base);

    if (self->t_mgr)
        hev_fsh_token_manager_reload (self->t_mgr);
}

static int
hev_fsh_server_socket (HevFshServer *self, HevFshConfig *config)
{
    struct sockaddr *addr;
    socklen_t addr_len;
    int reuse = 1;
    int fd;

    addr = hev_fsh_config_get_server_sockaddr (config, &addr_len);
    if (!addr) {
        LOG_E ("%p fsh server socket addr", self);
        return -1;
    }

    fd = hev_task_io_socket_socket (addr->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        LOG_E ("%p fsh server socket socket", self);
        return -1;
    }

    if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (int)) < 0) {
        LOG_E ("%p fsh server socket reuse addr", self);
        close (fd);
        return -1;
    }

    if (setsockopt (fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof (int)) < 0) {
        LOG_E ("%p fsh server socket reuse port", self);
        close (fd);
        return -1;
    }

    if (bind (fd, addr, addr_len) < 0) {
        LOG_E ("%p fsh server socket bind", self);
        close (fd);
        return -1;
    }

    if (listen (fd, 10) < 0) {
        LOG_E ("%p fsh server socket listen", self);
        close (fd);
        return -1;
    }

    return fd;
}

int
hev_fsh_server_construct (HevFshServer *self, HevFshConfig *config)
{
    const char *tokens_file;
    int res;

    res = hev_fsh_base_construct (&self->base);
    if (res < 0)
        return res;

    LOG_D ("%p fsh server construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_SERVER_TYPE;

    self->fd = hev_fsh_server_socket (self, config);
    if (self->fd < 0)
        return -1;

    self->main_task = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!self->main_task) {
        close (self->fd);
        return -1;
    }

    self->s_mgr = hev_fsh_session_manager_new ();
    if (!self->s_mgr) {
        hev_task_unref (self->main_task);
        close (self->fd);
        return -1;
    }

    tokens_file = hev_fsh_config_get_tokens_file (config);
    if (tokens_file) {
        self->event_task = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
        if (!self->event_task) {
            hev_object_unref (HEV_OBJECT (self->s_mgr));
            hev_task_unref (self->main_task);
            close (self->fd);
            return -1;
        }

        self->t_mgr = hev_fsh_token_manager_new (tokens_file);
        if (!self->t_mgr) {
            hev_task_unref (self->event_task);
            hev_object_unref (HEV_OBJECT (self->s_mgr));
            hev_task_unref (self->main_task);
            close (self->fd);
            return -1;
        }
        hev_fsh_token_manager_reload (self->t_mgr);

        res = hev_task_io_pipe_pipe (self->pfds);
        if (res < 0) {
            hev_object_unref (HEV_OBJECT (self->t_mgr));
            hev_task_unref (self->event_task);
            hev_object_unref (HEV_OBJECT (self->s_mgr));
            hev_task_unref (self->main_task);
            close (self->fd);
            return -1;
        }
    }

    self->config = config;

    return 0;
}

static void
hev_fsh_server_destruct (HevObject *base)
{
    HevFshServer *self = HEV_FSH_SERVER (base);

    LOG_D ("%p fsh server destruct", self);

    hev_object_unref (HEV_OBJECT (self->s_mgr));
    if (self->t_mgr)
        hev_object_unref (HEV_OBJECT (self->t_mgr));
    if (self->event_task) {
        hev_task_unref (self->event_task);
        close (self->pfds[0]);
        close (self->pfds[1]);
    }
    hev_task_unref (self->main_task);
    close (self->fd);

    HEV_FSH_BASE_TYPE->destruct (base);
}

HevObjectClass *
hev_fsh_server_class (void)
{
    static HevFshServerClass klass;
    HevFshServerClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshBaseClass *bkptr;

        memcpy (kptr, HEV_FSH_BASE_TYPE, sizeof (HevFshBaseClass));

        okptr->name = "HevFshServer";
        okptr->destruct = hev_fsh_server_destruct;

        bkptr = HEV_FSH_BASE_CLASS (kptr);
        bkptr->start = hev_fsh_server_start;
        bkptr->stop = hev_fsh_server_stop;
        bkptr->reload = hev_fsh_server_reload;
    }

    return okptr;
}
