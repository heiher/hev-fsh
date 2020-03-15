/*
 ============================================================================
 Name        : hev-fsh-client-base.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client base
 ============================================================================
 */

#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-call.h>
#include <hev-task-io-socket.h>

#include "hev-fsh-client-base.h"

typedef struct _HevTaskCallResolv HevTaskCallResolv;

struct _HevTaskCallResolv
{
    HevTaskCall base;

    HevFshConfig *config;
    struct sockaddr *addr;
};

static int
hev_fsh_client_base_socket (void)
{
    int fd, flags;

    fd = hev_task_io_socket_socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
        goto exit;

    flags = fcntl (fd, F_GETFD);
    if (flags < 0)
        goto exit_close;

    flags |= FD_CLOEXEC;
    if (fcntl (fd, F_SETFD, flags) < 0)
        goto exit_close;

    return fd;

exit_close:
    close (fd);
exit:
    return -1;
}

int
hev_fsh_client_base_construct (HevFshClientBase *self, HevFshConfig *config,
                               HevFshSessionManager *sm)
{
    self->fd = hev_fsh_client_base_socket ();
    if (self->fd < 0) {
        fprintf (stderr, "Create client's socket failed!\n");
        return -1;
    }

    self->_sm = sm;
    self->config = config;
    self->base.hp = HEV_FSH_SESSION_HP;
    hev_fsh_session_manager_insert (sm, &self->base);

    signal (SIGCHLD, SIG_IGN);

    return 0;
}

void
hev_fsh_client_base_destroy (HevFshClientBase *self)
{
    close (self->fd);
    hev_fsh_session_manager_remove (self->_sm, &self->base);

    if (self->_destroy)
        self->_destroy (self);
}

static void
resolv_entry (HevTaskCall *call)
{
    HevTaskCallResolv *resolv = (HevTaskCallResolv *)call;
    struct sockaddr_in *saddr;
    const char *addr;
    int port;

    addr = hev_fsh_config_get_server_address (resolv->config);
    port = hev_fsh_config_get_server_port (resolv->config);

    saddr = (struct sockaddr_in *)resolv->addr;
    saddr->sin_family = AF_INET;
    saddr->sin_addr.s_addr = inet_addr (addr);
    saddr->sin_port = htons (port);

    if (saddr->sin_addr.s_addr == INADDR_NONE) {
        struct hostent *h;

        h = gethostbyname (addr);
        if (!h) {
            hev_task_call_set_retval (call, NULL);
            return;
        }

        saddr->sin_family = h->h_addrtype;
        saddr->sin_addr.s_addr = *(in_addr_t *)h->h_addr;
    }

    hev_task_call_set_retval (call, saddr);
}

int
hev_fsh_client_base_resolv (HevFshClientBase *self, struct sockaddr *addr)
{
    HevTaskCall *call;
    HevTaskCallResolv *resolv;
    int ret = -1;

    call = hev_task_call_new (sizeof (HevTaskCallResolv), 16384);
    if (!call)
        return ret;

    resolv = (HevTaskCallResolv *)call;
    resolv->config = self->config;
    resolv->addr = addr;

    if (hev_task_call_jump (call, resolv_entry))
        ret = 0;

    hev_task_call_destroy (call);
    return ret;
}
