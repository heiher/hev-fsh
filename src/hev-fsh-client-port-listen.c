/*
 ============================================================================
 Name        : hev-fsh-client-port-listen.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client port connect
 ============================================================================
 */

#include <string.h>
#include <unistd.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-fsh-client-port-connect.h"

#include "hev-fsh-client-port-listen.h"

static void
hev_fsh_client_port_listen_dispatch (HevFshClientListen *base, int fd)
{
    HevFshClientBase *b = HEV_FSH_CLIENT_BASE (base);
    HevFshClientBase *client;

    client = hev_fsh_client_port_connect_new (b->config, fd);
    if (client)
        hev_fsh_io_run (HEV_FSH_IO (client));
    else
        close (fd);
}

HevFshClientBase *
hev_fsh_client_port_listen_new (HevFshConfig *config)
{
    HevFshClientPortListen *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClientPortListen));
    if (!self)
        return NULL;

    res = hev_fsh_client_port_listen_construct (self, config);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client port listen new", self);

    return HEV_FSH_CLIENT_BASE (self);
}

int
hev_fsh_client_port_listen_construct (HevFshClientPortListen *self,
                                      HevFshConfig *config)
{
    int res;

    res = hev_fsh_client_listen_construct (&self->base, config);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client port listen construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_PORT_LISTEN_TYPE;

    return 0;
}

static void
hev_fsh_client_port_listen_destruct (HevObject *base)
{
    HevFshClientPortListen *self = HEV_FSH_CLIENT_PORT_LISTEN (base);

    LOG_D ("%p fsh client port listen destruct", self);

    HEV_FSH_CLIENT_LISTEN_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_client_port_listen_class (void)
{
    static HevFshClientPortListenClass klass;
    HevFshClientPortListenClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshClientListenClass *ckptr;
        void *ptr;

        ptr = HEV_FSH_CLIENT_LISTEN_TYPE;
        memcpy (kptr, ptr, sizeof (HevFshClientListenClass));

        okptr->name = "HevFshClientPortListen";
        okptr->finalizer = hev_fsh_client_port_listen_destruct;

        ckptr = HEV_FSH_CLIENT_LISTEN_CLASS (kptr);
        ckptr->dispatch = hev_fsh_client_port_listen_dispatch;
    }

    return okptr;
}
