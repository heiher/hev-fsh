/*
 ============================================================================
 Name        : hev-fsh-session.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2021 xyz
 Description : Fsh session
 ============================================================================
 */

#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-compiler.h"
#include "hev-fsh-config.h"

#include "hev-fsh-session.h"

enum
{
    TYPE_NULL = 0,
    TYPE_FORWARD,
    TYPE_CONNECT,
    TYPE_SPLICE,
    TYPE_CLOSED,
};

static void
sleep_wait (unsigned int milliseconds)
{
    while (milliseconds)
        milliseconds = hev_task_sleep (milliseconds);
}

static void
hev_fsh_session_log (HevFshSession *self, const char *type)
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof (addr);
    const char *sa = NULL;
    int port = 0;
    char ts[64];
    char bs[64];
    int res;

    hev_fsh_protocol_token_to_string (self->token, ts);

    res = getpeername (self->client_fd, (struct sockaddr *)&addr, &len);
    if (res >= 0) {
        if (addr.ss_family == AF_INET) {
            struct sockaddr_in *p = (struct sockaddr_in *)&addr;
            sa = inet_ntop (AF_INET, &p->sin_addr, bs, sizeof (bs));
            port = ntohs (p->sin_port);
        } else if (addr.ss_family == AF_INET6) {
            struct sockaddr_in6 *p = (struct sockaddr_in6 *)&addr;
            sa = inet_ntop (AF_INET6, &p->sin6_addr, bs, sizeof (bs));
            port = ntohs (p->sin6_port);
        }
    }

    if (LOG_ON_D ())
        LOG_D ("%p fsh session %s %s [%s]:%d", self, type, ts, sa, port);
    else
        LOG_I ("%s %s [%s]:%d", type, ts, sa, port);
}

static int
hev_fsh_session_write_message (HevFshSession *self, int ver, int cmd,
                               void *data, size_t size)
{
    int fd = self->client_fd;
    struct msghdr mh = { 0 };
    struct iovec iov[2];
    HevFshMessage msg;
    int res;

    msg.ver = ver;
    msg.cmd = cmd;

    iov[0].iov_base = &msg;
    iov[0].iov_len = sizeof (msg);
    iov[1].iov_base = data;
    iov[1].iov_len = size;

    mh.msg_iov = iov;
    mh.msg_iovlen = 2;

    hev_task_mutex_lock (&self->wlock);
    res = hev_task_io_socket_sendmsg (fd, &mh, MSG_WAITALL, io_yielder, self);
    hev_task_mutex_unlock (&self->wlock);

    return res;
}

static int
hev_fsh_session_login (HevFshSession *self, int msg_ver)
{
    HevFshMessageToken mt;
    HevFshSession *s;
    int cmd;
    int res;

    if (self->type)
        return -1;

    if (msg_ver == 1) {
        hev_fsh_protocol_token_generate (self->token);
    } else {
        HevFshToken zt = { 0 };

        res = hev_task_io_socket_recv (self->client_fd, &mt, sizeof (mt),
                                       MSG_WAITALL, io_yielder, self);
        if (res <= 0)
            return -1;

        if (memcmp (zt, mt.token, sizeof (HevFshToken)) == 0) {
            hev_fsh_protocol_token_generate (self->token);
        } else {
            memcpy (self->token, mt.token, sizeof (HevFshToken));
        }
    }

    s = hev_fsh_session_manager_find (self->manager, TYPE_FORWARD,
                                      &self->token);
    if (s) {
        HevFshIO *io = HEV_FSH_IO (s);

        s->is_mgr = 0;
        s->type = TYPE_CLOSED;
        hev_fsh_session_manager_remove (s->manager, s);

        io->timeout = 0;
        hev_task_wakeup (io->task);
    }

    cmd = HEV_FSH_CMD_TOKEN;
    memcpy (mt.token, self->token, sizeof (HevFshToken));
    res = hev_fsh_session_write_message (self, 1, cmd, &mt, sizeof (mt));
    if (res <= 0)
        return -1;

    self->is_mgr = 1;
    self->type = TYPE_FORWARD;
    self->is_temp_token = (msg_ver == 3) ? 1 : 0;
    hev_fsh_session_manager_insert (self->manager, self);
    hev_fsh_session_log (self, "L");

    return 0;
}

static void
hev_fsh_session_splice (HevFshSession *self)
{
    unsigned timeout = self->base.timeout;

    /* wait for accept */
    while (timeout) {
        if (self->remote_fd >= 0)
            break;
        timeout = hev_task_sleep (timeout);
    }

    hev_task_io_splice (self->client_fd, self->client_fd, self->remote_fd,
                        self->remote_fd, 8192, io_yielder, self);
}

static int
hev_fsh_session_connect (HevFshSession *self)
{
    HevFshMessageToken mt;
    HevFshSession *s;
    int cmd;
    int res;

    if (self->type)
        return -1;

    res = hev_task_io_socket_recv (self->client_fd, &mt, sizeof (mt),
                                   MSG_WAITALL, io_yielder, self);
    if (res <= 0)
        return -1;

    s = hev_fsh_session_manager_find (self->manager, TYPE_FORWARD, &mt.token);
    if (!s) {
        sleep_wait (1500);
        return -1;
    }

    if (s->is_temp_token)
        hev_fsh_protocol_token_generate (mt.token);

    cmd = HEV_FSH_CMD_CONNECT;
    res = hev_fsh_session_write_message (s, 1, cmd, &mt, sizeof (mt));
    if (res <= 0)
        return -1;

    self->is_mgr = 1;
    self->type = TYPE_CONNECT;
    memcpy (self->token, mt.token, sizeof (HevFshToken));
    hev_fsh_session_manager_insert (self->manager, self);
    hev_fsh_session_log (self, "C");

    hev_fsh_session_splice (self);

    return -1;
}

static int
hev_fsh_session_accept (HevFshSession *self)
{
    HevFshSessionManager *manager = self->manager;
    HevFshSession *s = NULL;
    HevFshMessageToken mt;
    unsigned timeout;
    int res;

    res = hev_task_io_socket_recv (self->client_fd, &mt, sizeof (mt),
                                   MSG_WAITALL, io_yielder, self);
    if (res <= 0)
        return -1;

    /* wait for connect */
    timeout = self->base.timeout;
    while (timeout) {
        s = hev_fsh_session_manager_find (manager, TYPE_CONNECT, &mt.token);
        if (s)
            break;
        timeout = hev_task_sleep (timeout);
    }

    if (!s)
        return -1;

    s->type = TYPE_SPLICE;
    s->remote_fd = self->client_fd;
    hev_fsh_session_manager_remove (manager, s);
    hev_fsh_session_manager_insert (manager, s);
    hev_task_del_fd (hev_task_self (), self->client_fd);
    hev_task_add_fd (HEV_FSH_IO (s)->task, s->remote_fd, POLLIN | POLLOUT);
    hev_task_wakeup (HEV_FSH_IO (s)->task);

    self->client_fd = -1;
    return -1;
}

static int
hev_fsh_session_keep_alive (HevFshSession *self, int msg_ver)
{
    HevFshMessage msg;
    int res;

    if (msg_ver == 1)
        return 0;

    msg.ver = 1;
    msg.cmd = HEV_FSH_CMD_KEEP_ALIVE;

    hev_task_mutex_lock (&self->wlock);
    res = hev_task_io_socket_send (self->client_fd, &msg, sizeof (msg),
                                   MSG_WAITALL, io_yielder, self);
    hev_task_mutex_unlock (&self->wlock);
    if (res <= 0)
        return -1;

    return 0;
}

static void
hev_fsh_session_close_session (HevFshSession *self)
{
    if (self->type)
        hev_fsh_session_log (self, "D");

    if (self->is_mgr)
        hev_fsh_session_manager_remove (self->manager, self);

    hev_object_unref (HEV_OBJECT (self));
}

static void
hev_fsh_session_task_entry (void *data)
{
    HevFshSession *self = data;

    hev_task_add_fd (hev_task_self (), self->client_fd, POLLIN | POLLOUT);

    for (;;) {
        HevFshMessage msg;
        int res;

        res = hev_task_io_socket_recv (self->client_fd, &msg, sizeof (msg),
                                       MSG_WAITALL, io_yielder, self);

        if (res < 0) {
            hev_fsh_session_close_session (self);
            break;
        }

        switch (msg.cmd) {
        case HEV_FSH_CMD_LOGIN:
            res = hev_fsh_session_login (self, msg.ver);
            break;
        case HEV_FSH_CMD_CONNECT:
            res = hev_fsh_session_connect (self);
            break;
        case HEV_FSH_CMD_ACCEPT:
            res = hev_fsh_session_accept (self);
            break;
        case HEV_FSH_CMD_KEEP_ALIVE:
            res = hev_fsh_session_keep_alive (self, msg.ver);
            break;
        default:
            res = -1;
        }

        if (res < 0) {
            hev_fsh_session_close_session (self);
            break;
        }
    }
}

void
hev_fsh_session_run (HevFshIO *base)
{
    LOG_D ("%p fsh session run", base);

    hev_task_run (base->task, hev_fsh_session_task_entry, base);
}

HevFshSession *
hev_fsh_session_new (int fd, unsigned int timeout,
                     HevFshSessionManager *manager)
{
    HevFshSession *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshSession));
    if (!self)
        return NULL;

    res = hev_fsh_session_construct (self, fd, timeout, manager);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh session new", self);

    return self;
}

int
hev_fsh_session_construct (HevFshSession *self, int fd, unsigned int timeout,
                           HevFshSessionManager *manager)
{
    int res;

    res = hev_fsh_io_construct (&self->base, timeout);
    if (res < 0)
        return res;

    LOG_D ("%p fsh session construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_SESSION_TYPE;

    self->client_fd = fd;
    self->remote_fd = -1;
    self->manager = manager;

    return 0;
}

static void
hev_fsh_session_destruct (HevObject *base)
{
    HevFshSession *self = HEV_FSH_SESSION (base);

    LOG_D ("%p fsh session destruct", self);

    if (self->remote_fd >= 0)
        close (self->remote_fd);
    if (self->client_fd >= 0)
        close (self->client_fd);

    HEV_FSH_IO_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_session_class (void)
{
    static HevFshSessionClass klass;
    HevFshSessionClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshIOClass *ikptr;

        memcpy (kptr, HEV_FSH_IO_TYPE, sizeof (HevFshIOClass));

        okptr->name = "HevFshSession";
        okptr->finalizer = hev_fsh_session_destruct;

        ikptr = HEV_FSH_IO_CLASS (kptr);
        ikptr->run = hev_fsh_session_run;
    }

    return okptr;
}
