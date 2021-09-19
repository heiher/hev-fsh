/*
 ============================================================================
 Name        : hev-fsh-client-sock-listen.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh client sock connect
 ============================================================================
 */

#include <string.h>
#include <unistd.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-fsh-client-sock-connect.h"

#include "hev-fsh-client-sock-listen.h"

static void
hev_fsh_client_sock_listen_dispatch (HevFshClientListen *base, int fd)
{
    HevFshClientBase *b = HEV_FSH_CLIENT_BASE (base);
    HevFshClientBase *client;

    client = hev_fsh_client_sock_connect_new (b->config, fd);
    if (client)
        hev_fsh_io_run (HEV_FSH_IO (client));
    else
        close (fd);
}

HevFshClientBase *
hev_fsh_client_sock_listen_new (HevFshConfig *config)
{
    HevFshClientSockListen *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClientSockListen));
    if (!self)
        return NULL;

    res = hev_fsh_client_sock_listen_construct (self, config);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client sock listen new", self);

    return HEV_FSH_CLIENT_BASE (self);
}

int
hev_fsh_client_sock_listen_construct (HevFshClientSockListen *self,
                                      HevFshConfig *config)
{
    int res;

    res = hev_fsh_client_listen_construct (&self->base, config);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client sock listen construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_SOCK_LISTEN_TYPE;

    return 0;
}

static void
hev_fsh_client_sock_listen_destruct (HevObject *base)
{
    HevFshClientSockListen *self = HEV_FSH_CLIENT_SOCK_LISTEN (base);

    LOG_D ("%p fsh client sock listen destruct", self);

    HEV_FSH_CLIENT_LISTEN_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_client_sock_listen_class (void)
{
    static HevFshClientSockListenClass klass;
    HevFshClientSockListenClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshClientListenClass *ckptr;
        void *ptr;

        ptr = HEV_FSH_CLIENT_LISTEN_TYPE;
        memcpy (kptr, ptr, sizeof (HevFshClientListenClass));

        okptr->name = "HevFshClientSockListen";
        okptr->finalizer = hev_fsh_client_sock_listen_destruct;

        ckptr = HEV_FSH_CLIENT_LISTEN_CLASS (kptr);
        ckptr->dispatch = hev_fsh_client_sock_listen_dispatch;
    }

    return okptr;
}
