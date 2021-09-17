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
    TYPE_FORWARD = 1,
    TYPE_CONNECT,
    TYPE_SPLICE,
    TYPE_CLOSED,
};

enum
{
    STEP_NULL,
    STEP_READ_MESSAGE,
    STEP_DO_LOGIN,
    STEP_WRITE_MESSAGE_TOKEN,
    STEP_DO_CONNECT,
    STEP_WRITE_MESSAGE_CONNECT,
    STEP_DO_SPLICE,
    STEP_DO_ACCEPT,
    STEP_DO_KEEP_ALIVE,
    STEP_CLOSE_SESSION,
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
    socklen_t addr_len;
    const char *sa;
    char token[40];
    char buf[64];
    int port;

    hev_fsh_protocol_token_to_string (self->token, token);

    port = 0;
    sa = NULL;
    addr_len = sizeof (addr);
    __builtin_bzero (&addr, addr_len);
    getpeername (self->client_fd, (struct sockaddr *)&addr, &addr_len);
    switch (addr.ss_family) {
    case AF_INET: {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;
        sa = inet_ntop (AF_INET, &addr4->sin_addr, buf, sizeof (buf));
        port = ntohs (addr4->sin_port);
        break;
    }
    case AF_INET6: {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;
        sa = inet_ntop (AF_INET6, &addr6->sin6_addr, buf, sizeof (buf));
        port = ntohs (addr6->sin6_port);
        break;
    }
    }

    if (LOG_ON_D ())
        LOG_D ("%p fsh session %s %s [%s]:%d", self, type, token, sa, port);
    else
        LOG_I ("%s %s [%s]:%d", type, token, sa, port);
}

static int
hev_fsh_session_read_message (HevFshSession *self)
{
    HevFshMessage msg;

    if (hev_task_io_socket_recv (self->client_fd, &msg, sizeof (msg),
                                 MSG_WAITALL, io_yielder, self) <= 0)
        return STEP_CLOSE_SESSION;

    self->msg_ver = msg.ver;

    switch (msg.cmd) {
    case HEV_FSH_CMD_LOGIN:
        return STEP_DO_LOGIN;
    case HEV_FSH_CMD_CONNECT:
        return STEP_DO_CONNECT;
    case HEV_FSH_CMD_ACCEPT:
        return STEP_DO_ACCEPT;
    case HEV_FSH_CMD_KEEP_ALIVE:
        return STEP_DO_KEEP_ALIVE;
    }

    return STEP_CLOSE_SESSION;
}

static int
hev_fsh_session_do_login (HevFshSession *self)
{
    if (self->msg_ver == 1) {
        hev_fsh_protocol_token_generate (self->token);
    } else {
        HevFshMessageToken msg_token;
        HevFshToken zero_token = { 0 };

        if (hev_task_io_socket_recv (self->client_fd, &msg_token,
                                     sizeof (msg_token), MSG_WAITALL,
                                     io_yielder, self) <= 0)
            return STEP_CLOSE_SESSION;

        if (memcmp (zero_token, msg_token.token, sizeof (HevFshToken)) == 0) {
            hev_fsh_protocol_token_generate (self->token);
        } else {
            HevFshSession *s;

            s = hev_fsh_session_manager_find (self->manager, TYPE_FORWARD,
                                              &msg_token.token);
            if (s) {
                s->type = TYPE_CLOSED;
                HEV_FSH_IO (s)->timeout = 0;
                hev_fsh_session_manager_remove (self->manager, s);
                hev_fsh_session_manager_insert (self->manager, s);
                hev_task_wakeup (HEV_FSH_IO (s)->task);
            }

            memcpy (self->token, msg_token.token, sizeof (HevFshToken));
        }
    }

    if (self->type)
        return STEP_CLOSE_SESSION;

    self->type = TYPE_FORWARD;
    hev_fsh_session_manager_insert (self->manager, self);
    hev_fsh_session_log (self, "L");

    return STEP_WRITE_MESSAGE_TOKEN;
}

static int
hev_fsh_session_write_message_token (HevFshSession *self)
{
    HevFshMessage msg;
    HevFshMessageToken msg_token;
    struct iovec iov[2];
    struct msghdr mh;
    ssize_t res;

    msg.ver = 1;
    msg.cmd = HEV_FSH_CMD_TOKEN;
    memcpy (msg_token.token, self->token, sizeof (HevFshToken));

    iov[0].iov_base = &msg;
    iov[0].iov_len = sizeof (msg);
    iov[1].iov_base = &msg_token;
    iov[1].iov_len = sizeof (msg_token);

    __builtin_bzero (&mh, sizeof (mh));
    mh.msg_iov = iov;
    mh.msg_iovlen = 2;

    hev_task_mutex_lock (&self->wlock);
    res = hev_task_io_socket_sendmsg (self->client_fd, &mh, MSG_WAITALL,
                                      io_yielder, self);
    hev_task_mutex_unlock (&self->wlock);
    if (res <= 0)
        return STEP_CLOSE_SESSION;

    return STEP_READ_MESSAGE;
}

static int
hev_fsh_session_do_connect (HevFshSession *self)
{
    HevFshMessageToken msg_token;

    if (hev_task_io_socket_recv (self->client_fd, &msg_token,
                                 sizeof (msg_token), MSG_WAITALL, io_yielder,
                                 self) <= 0)
        return STEP_CLOSE_SESSION;

    if (self->type)
        return STEP_CLOSE_SESSION;

    self->type = TYPE_CONNECT;
    memcpy (self->token, msg_token.token, sizeof (HevFshToken));
    hev_fsh_session_manager_insert (self->manager, self);
    hev_fsh_session_log (self, "C");

    return STEP_WRITE_MESSAGE_CONNECT;
}

static int
hev_fsh_session_write_message_connect (HevFshSession *self)
{
    HevFshMessageToken msg_token;
    HevFshMessage msg;
    HevFshSession *s;
    struct iovec iov[2];
    struct msghdr mh;
    ssize_t res;

    s = hev_fsh_session_manager_find (self->manager, TYPE_FORWARD,
                                      &self->token);
    if (!s) {
        sleep_wait (1500);
        return STEP_CLOSE_SESSION;
    }

    msg.ver = self->msg_ver;
    msg.cmd = HEV_FSH_CMD_CONNECT;
    memcpy (msg_token.token, self->token, sizeof (HevFshToken));

    iov[0].iov_base = &msg;
    iov[0].iov_len = sizeof (msg);
    iov[1].iov_base = &msg_token;
    iov[1].iov_len = sizeof (msg_token);

    __builtin_bzero (&mh, sizeof (mh));
    mh.msg_iov = iov;
    mh.msg_iovlen = 2;

    hev_task_mutex_lock (&s->wlock);
    res = hev_task_io_socket_sendmsg (s->client_fd, &mh, MSG_WAITALL,
                                      io_yielder, self);
    hev_task_mutex_unlock (&s->wlock);
    if (res <= 0)
        return STEP_CLOSE_SESSION;

    return STEP_DO_SPLICE;
}

static int
hev_fsh_session_do_accept (HevFshSession *self)
{
    HevFshMessageToken msg_token;
    HevFshSession *s;

    if (hev_task_io_socket_recv (self->client_fd, &msg_token,
                                 sizeof (msg_token), MSG_WAITALL, io_yielder,
                                 self) <= 0)
        return STEP_CLOSE_SESSION;

    s = hev_fsh_session_manager_find (self->manager, TYPE_CONNECT,
                                      &msg_token.token);
    if (!s) {
        sleep_wait (1500);
        return STEP_CLOSE_SESSION;
    }

    s->type = TYPE_SPLICE;
    s->remote_fd = self->client_fd;
    hev_fsh_session_manager_remove (self->manager, s);
    hev_fsh_session_manager_insert (self->manager, s);
    hev_task_del_fd (hev_task_self (), self->client_fd);
    hev_task_add_fd (HEV_FSH_IO (s)->task, s->remote_fd, POLLIN | POLLOUT);
    hev_task_wakeup (HEV_FSH_IO (s)->task);

    self->client_fd = -1;
    return STEP_CLOSE_SESSION;
}

static int
hev_fsh_session_do_keep_alive (HevFshSession *self)
{
    HevFshMessage msg;
    ssize_t res;

    if (self->msg_ver == 1)
        goto exit;

    msg.ver = 1;
    msg.cmd = HEV_FSH_CMD_KEEP_ALIVE;

    hev_task_mutex_lock (&self->wlock);
    res = hev_task_io_socket_send (self->client_fd, &msg, sizeof (msg),
                                   MSG_WAITALL, io_yielder, self);
    hev_task_mutex_unlock (&self->wlock);
    if (res <= 0)
        return STEP_CLOSE_SESSION;

exit:
    return STEP_READ_MESSAGE;
}

static int
hev_fsh_session_do_splice (HevFshSession *self)
{
    unsigned timeout = self->base.timeout;

    /* wait for remote fd */
    while (timeout) {
        if (self->remote_fd >= 0)
            break;
        timeout = hev_task_sleep (timeout);
    }

    hev_task_io_splice (self->client_fd, self->client_fd, self->remote_fd,
                        self->remote_fd, 8192, io_yielder, self);

    return STEP_CLOSE_SESSION;
}

static int
hev_fsh_session_close_session (HevFshSession *self)
{
    if (self->type) {
        hev_fsh_session_manager_remove (self->manager, self);
        hev_fsh_session_log (self, "D");
    }

    hev_object_unref (HEV_OBJECT (self));

    return STEP_NULL;
}

static void
hev_fsh_session_task_entry (void *data)
{
    HevFshSession *self = data;
    int step = STEP_READ_MESSAGE;

    hev_task_add_fd (hev_task_self (), self->client_fd, POLLIN | POLLOUT);

    while (step) {
        switch (step) {
        case STEP_READ_MESSAGE:
            step = hev_fsh_session_read_message (self);
            break;
        case STEP_DO_LOGIN:
            step = hev_fsh_session_do_login (self);
            break;
        case STEP_WRITE_MESSAGE_TOKEN:
            step = hev_fsh_session_write_message_token (self);
            break;
        case STEP_DO_CONNECT:
            step = hev_fsh_session_do_connect (self);
            break;
        case STEP_WRITE_MESSAGE_CONNECT:
            step = hev_fsh_session_write_message_connect (self);
            break;
        case STEP_DO_SPLICE:
            step = hev_fsh_session_do_splice (self);
            break;
        case STEP_DO_ACCEPT:
            step = hev_fsh_session_do_accept (self);
            break;
        case STEP_DO_KEEP_ALIVE:
            step = hev_fsh_session_do_keep_alive (self);
            break;
        case STEP_CLOSE_SESSION:
            step = hev_fsh_session_close_session (self);
            break;
        default:
            step = STEP_NULL;
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
