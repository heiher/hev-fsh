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
hev_fsh_client_port_listen_task_entry (void *data)
{
    HevFshClientBase *base = data;
    int res;

    res = hev_fsh_client_base_listen (base);
    if (res < 0) {
        LOG_E ("%p fsh client port listen listen", base);
        goto exit;
    }

    for (;;) {
        HevFshClientBase *client;
        int fd;

        fd = hev_task_io_socket_accept (base->fd, NULL, NULL, NULL, NULL);
        if (fd < 0)
            continue;

        client = hev_fsh_client_port_connect_new (base->config, fd);
        if (client)
            hev_fsh_io_run (HEV_FSH_IO (client));
        else
            close (fd);
    }

exit:
    hev_object_unref (HEV_OBJECT (base));
}

static void
hev_fsh_client_port_listen_run (HevFshIO *base)
{
    LOG_D ("%p fsh client port listen run", base);

    hev_task_run (base->task, hev_fsh_client_port_listen_task_entry, base);
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

    res = hev_fsh_client_base_construct (&self->base, config);
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

    HEV_FSH_CLIENT_BASE_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_client_port_listen_class (void)
{
    static HevFshClientPortListenClass klass;
    HevFshClientPortListenClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshIOClass *ikptr;

        memcpy (kptr, HEV_FSH_CLIENT_BASE_TYPE, sizeof (HevFshClientBaseClass));

        okptr->name = "HevFshClientPortListen";
        okptr->finalizer = hev_fsh_client_port_listen_destruct;

        ikptr = HEV_FSH_IO_CLASS (kptr);
        ikptr->run = hev_fsh_client_port_listen_run;
    }

    return okptr;
}
