/*
 ============================================================================
 Name        : hev-fsh-client-sock-accept.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh client sock accept
 ============================================================================
 */

#include <string.h>
#include <unistd.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-socks5-server.h"

#include "hev-fsh-client-sock-accept.h"

static void
hev_fsh_client_sock_accept_task_entry (void *data)
{
    HevFshClientSockAccept *self = data;
    HevFshClientBase *base = data;
    HevSocks5Server *socks;
    int res;

    res = hev_fsh_client_accept_send_accept (&self->base);
    if (res < 0)
        goto quit;

    socks = hev_socks5_server_new (base->fd);
    if (!socks)
        goto quit;

    hev_socks5_server_run (socks);
    hev_object_unref (HEV_OBJECT (socks));

quit:
    hev_object_unref (HEV_OBJECT (self));
}

static void
hev_fsh_client_sock_accept_run (HevFshIO *base)
{
    LOG_D ("%p fsh client sock accept run", base);

    hev_task_run (base->task, hev_fsh_client_sock_accept_task_entry, base);
}

HevFshClientBase *
hev_fsh_client_sock_accept_new (HevFshConfig *config, HevFshToken token)
{
    HevFshClientSockAccept *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClientSockAccept));
    if (!self)
        return NULL;

    res = hev_fsh_client_sock_accept_construct (self, config, token);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client sock accept new", self);

    return HEV_FSH_CLIENT_BASE (self);
}

int
hev_fsh_client_sock_accept_construct (HevFshClientSockAccept *self,
                                      HevFshConfig *config, HevFshToken token)
{
    int res;

    res = hev_fsh_client_accept_construct (&self->base, config, token);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client sock accept construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_SOCK_ACCEPT_TYPE;

    return 0;
}

static void
hev_fsh_client_sock_accept_destruct (HevObject *base)
{
    HevFshClientSockAccept *self = HEV_FSH_CLIENT_SOCK_ACCEPT (base);

    LOG_D ("%p fsh client sock accept destruct", self);

    HEV_FSH_CLIENT_ACCEPT_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_client_sock_accept_class (void)
{
    static HevFshClientSockAcceptClass klass;
    HevFshClientSockAcceptClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshIOClass *ikptr;
        void *ptr;

        ptr = HEV_FSH_CLIENT_ACCEPT_TYPE;
        memcpy (kptr, ptr, sizeof (HevFshClientAcceptClass));

        okptr->name = "HevFshClientSockAccept";
        okptr->finalizer = hev_fsh_client_sock_accept_destruct;

        ikptr = HEV_FSH_IO_CLASS (kptr);
        ikptr->run = hev_fsh_client_sock_accept_run;
    }

    return okptr;
}
