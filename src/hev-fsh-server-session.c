/*
 ============================================================================
 Name        : hev-fsh-server-session.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2020 everyone.
 Description : Fsh server session
 ============================================================================
 */

#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-rbtree.h"
#include "hev-compiler.h"
#include "hev-fsh-config.h"
#include "hev-fsh-protocol.h"

#include "hev-fsh-server-session.h"

#define fsh_task_io_yielder hev_fsh_session_task_io_yielder

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

struct _HevFshServerSession
{
    HevFshSession base;
    HevRBTreeNode node;

    int client_fd;
    int remote_fd;

    int type;
    int msg_ver;
    HevFshToken token;
    HevRBTree *sessions_tree;

    HevFshSessionNotify notify;
    void *notify_data;
};

static void hev_fsh_server_session_task_entry (void *data);

HevFshServerSession *
hev_fsh_server_session_new (int client_fd, HevFshSessionNotify notify,
                            void *notify_data, HevRBTree *sessions_tree)
{
    HevFshServerSession *self;
    HevTask *task;

    self = hev_malloc0 (sizeof (HevFshServerSession));
    if (!self)
        goto exit;

    task = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!task)
        goto exit_free;

    self->remote_fd = -1;
    self->client_fd = client_fd;
    self->notify = notify;
    self->notify_data = notify_data;
    self->base.task = task;
    self->base.hp = HEV_FSH_SESSION_HP;
    self->sessions_tree = sessions_tree;
    hev_task_set_priority (task, 9);
    return self;

exit_free:
    hev_free (self);
exit:
    return NULL;
}

static inline void
hev_fsh_server_session_destroy (HevFshServerSession *self)
{
    hev_free (self);
}

void
hev_fsh_server_session_run (HevFshServerSession *self)
{
    hev_task_run (self->base.task, hev_fsh_server_session_task_entry, self);
}

static void
fsh_server_session_tree_insert (HevRBTree *tree, HevFshSession *s)
{
    HevFshServerSession *ss = HEV_FSH_SERVER_SESSION (s);
    HevRBTreeNode **n = &tree->root, *parent = NULL;

    while (*n) {
        HevFshServerSession *t = container_of (*n, HevFshServerSession, node);

        parent = *n;

        if (ss->type < t->type) {
            n = &((*n)->left);
        } else if (ss->type > t->type) {
            n = &((*n)->right);
        } else {
            if (memcmp (ss->token, t->token, sizeof (HevFshToken)) < 0)
                n = &((*n)->left);
            else
                n = &((*n)->right);
        }
    }

    hev_rbtree_node_link (&ss->node, parent, n);
    hev_rbtree_insert_color (tree, &ss->node);
}

static void
fsh_server_session_tree_remove (HevRBTree *tree, HevFshSession *s)
{
    HevFshServerSession *ss = HEV_FSH_SERVER_SESSION (s);

    hev_rbtree_erase (tree, &ss->node);
}

static HevFshSession *
fsh_server_session_tree_find (HevRBTree *tree, int type, HevFshToken *token)
{
    HevRBTreeNode **n = &tree->root;

    while (*n) {
        HevFshServerSession *t = container_of (*n, HevFshServerSession, node);

        if (type < t->type) {
            n = &((*n)->left);
        } else if (type > t->type) {
            n = &((*n)->right);
        } else {
            int res = memcmp (*token, t->token, sizeof (HevFshToken));
            if (res < 0)
                n = &((*n)->left);
            else if (res > 0)
                n = &((*n)->right);
            else
                return &t->base;
        }
    }

    return NULL;
}

static void
sleep_wait (unsigned int milliseconds)
{
    while (milliseconds)
        milliseconds = hev_task_sleep (milliseconds);
}

static void
fsh_server_log (HevFshServerSession *self, const char *type)
{
    struct sockaddr_storage addr;
    socklen_t addr_len;
    struct tm *info;
    time_t rawtime;
    const char *sa;
    char token[40];
    char buf[64];
    int port;

    time (&rawtime);
    info = localtime (&rawtime);
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

    printf ("[%04d-%02d-%02d %02d:%02d:%02d] %s %s [%s]:%d\n",
            1900 + info->tm_year, info->tm_mon + 1, info->tm_mday,
            info->tm_hour, info->tm_min, info->tm_sec, type, token, sa, port);
    fflush (stdout);
}

static int
fsh_server_read_message (HevFshServerSession *self)
{
    HevFshMessage msg;

    if (hev_task_io_socket_recv (self->client_fd, &msg, sizeof (msg),
                                 MSG_WAITALL, fsh_task_io_yielder, self) <= 0)
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
fsh_server_do_login (HevFshServerSession *self)
{
    if (self->msg_ver == 1) {
        hev_fsh_protocol_token_generate (self->token);
    } else {
        HevFshMessageToken msg_token;
        HevFshToken zero_token = { 0 };

        if (hev_task_io_socket_recv (self->client_fd, &msg_token,
                                     sizeof (msg_token), MSG_WAITALL,
                                     fsh_task_io_yielder, self) <= 0)
            return STEP_CLOSE_SESSION;

        if (memcmp (zero_token, msg_token.token, sizeof (HevFshToken)) == 0) {
            hev_fsh_protocol_token_generate (self->token);
        } else {
            HevFshSession *s;

            s = fsh_server_session_tree_find (self->sessions_tree, TYPE_FORWARD,
                                              &msg_token.token);
            if (s) {
                s->hp = 0;
                HEV_FSH_SERVER_SESSION (s)->type = TYPE_CLOSED;
                fsh_server_session_tree_remove (self->sessions_tree, s);
                fsh_server_session_tree_insert (self->sessions_tree, s);
                hev_task_wakeup (s->task);
            }

            memcpy (self->token, msg_token.token, sizeof (HevFshToken));
        }
    }

    self->type = TYPE_FORWARD;
    fsh_server_session_tree_insert (self->sessions_tree, &self->base);
    fsh_server_log (self, "L");

    return STEP_WRITE_MESSAGE_TOKEN;
}

static int
fsh_server_write_message_token (HevFshServerSession *self)
{
    HevFshMessage msg;
    HevFshMessageToken msg_token;
    struct iovec iov[2];
    struct msghdr mh;

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

    if (hev_task_io_socket_sendmsg (self->client_fd, &mh, MSG_WAITALL,
                                    fsh_task_io_yielder, self) <= 0)
        return STEP_CLOSE_SESSION;

    return STEP_READ_MESSAGE;
}

static int
fsh_server_do_connect (HevFshServerSession *self)
{
    HevFshMessageToken msg_token;

    if (hev_task_io_socket_recv (self->client_fd, &msg_token,
                                 sizeof (msg_token), MSG_WAITALL,
                                 fsh_task_io_yielder, self) <= 0)
        return STEP_CLOSE_SESSION;

    self->type = TYPE_CONNECT;
    memcpy (self->token, msg_token.token, sizeof (HevFshToken));
    fsh_server_session_tree_insert (self->sessions_tree, &self->base);
    fsh_server_log (self, "C");

    return STEP_WRITE_MESSAGE_CONNECT;
}

static int
fsh_server_write_message_connect (HevFshServerSession *self)
{
    HevFshSession *s;
    HevFshServerSession *ss;
    HevFshMessage msg;
    HevFshMessageToken msg_token;
    struct iovec iov[2];
    struct msghdr mh;

    s = fsh_server_session_tree_find (self->sessions_tree, TYPE_FORWARD,
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

    ss = HEV_FSH_SERVER_SESSION (s);
    if (hev_task_io_socket_sendmsg (ss->client_fd, &mh, MSG_WAITALL,
                                    fsh_task_io_yielder, self) <= 0)
        return STEP_CLOSE_SESSION;

    return STEP_DO_SPLICE;
}

static int
fsh_server_do_accept (HevFshServerSession *self)
{
    HevFshSession *s;
    HevFshServerSession *ss;
    HevFshMessageToken msg_token;

    if (hev_task_io_socket_recv (self->client_fd, &msg_token,
                                 sizeof (msg_token), MSG_WAITALL,
                                 fsh_task_io_yielder, self) <= 0)
        return STEP_CLOSE_SESSION;

    s = fsh_server_session_tree_find (self->sessions_tree, TYPE_CONNECT,
                                      &msg_token.token);
    if (!s) {
        sleep_wait (1500);
        return STEP_CLOSE_SESSION;
    }

    ss = HEV_FSH_SERVER_SESSION (s);
    ss->type = TYPE_SPLICE;
    ss->remote_fd = self->client_fd;
    fsh_server_session_tree_remove (self->sessions_tree, s);
    fsh_server_session_tree_insert (self->sessions_tree, s);
    hev_task_del_fd (hev_task_self (), self->client_fd);
    hev_task_add_fd (s->task, ss->remote_fd, POLLIN | POLLOUT);
    hev_task_wakeup (s->task);

    self->client_fd = -1;
    return STEP_CLOSE_SESSION;
}

static int
fsh_server_do_keep_alive (HevFshServerSession *self)
{
    HevFshMessage msg;

    if (self->msg_ver == 1)
        goto exit;

    msg.ver = 1;
    msg.cmd = HEV_FSH_CMD_KEEP_ALIVE;

    if (hev_task_io_socket_send (self->client_fd, &msg, sizeof (msg),
                                 MSG_WAITALL, fsh_task_io_yielder, self) <= 0)
        return STEP_CLOSE_SESSION;

exit:
    return STEP_READ_MESSAGE;
}

static int
fsh_server_do_splice (HevFshServerSession *self)
{
    /* wait for remote fd */
    while (self->base.hp > 0) {
        if (self->remote_fd >= 0)
            break;
        hev_task_yield (HEV_TASK_WAITIO);
    }

    hev_task_io_splice (self->client_fd, self->client_fd, self->remote_fd,
                        self->remote_fd, 8192, fsh_task_io_yielder, self);

    return STEP_CLOSE_SESSION;
}

static int
fsh_server_close_session (HevFshServerSession *self)
{
    if (self->type) {
        fsh_server_session_tree_remove (self->sessions_tree, &self->base);
        fsh_server_log (self, "D");
    }

    if (self->remote_fd >= 0)
        close (self->remote_fd);
    if (self->client_fd >= 0)
        close (self->client_fd);

    self->notify (&self->base, self->notify_data);
    hev_fsh_server_session_destroy (self);

    return STEP_NULL;
}

static void
hev_fsh_server_session_task_entry (void *data)
{
    HevFshServerSession *self = data;
    int step = STEP_READ_MESSAGE;

    hev_task_add_fd (hev_task_self (), self->client_fd, POLLIN | POLLOUT);

    while (step) {
        switch (step) {
        case STEP_READ_MESSAGE:
            step = fsh_server_read_message (self);
            break;
        case STEP_DO_LOGIN:
            step = fsh_server_do_login (self);
            break;
        case STEP_WRITE_MESSAGE_TOKEN:
            step = fsh_server_write_message_token (self);
            break;
        case STEP_DO_CONNECT:
            step = fsh_server_do_connect (self);
            break;
        case STEP_WRITE_MESSAGE_CONNECT:
            step = fsh_server_write_message_connect (self);
            break;
        case STEP_DO_SPLICE:
            step = fsh_server_do_splice (self);
            break;
        case STEP_DO_ACCEPT:
            step = fsh_server_do_accept (self);
            break;
        case STEP_DO_KEEP_ALIVE:
            step = fsh_server_do_keep_alive (self);
            break;
        case STEP_CLOSE_SESSION:
            step = fsh_server_close_session (self);
            break;
        default:
            step = STEP_NULL;
            break;
        }
    }
}
