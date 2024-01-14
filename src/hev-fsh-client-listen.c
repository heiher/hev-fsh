/*
 ============================================================================
 Name        : hev-fsh-client-listen.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh client connect
 ============================================================================
 */

#include <string.h>
#include <unistd.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"

#include "hev-fsh-client-listen.h"

static void
hev_fsh_client_listen_task_entry (void *data)
{
    HevFshClientListenClass *klass = HEV_OBJECT_GET_CLASS (data);
    HevFshClientListen *self = data;
    HevFshClientBase *base = data;
    int res;

    res = hev_fsh_client_base_listen (base);
    if (res < 0) {
        LOG_E ("%p fsh client listen listen", base);
        goto exit;
    }

    for (;;) {
        int fd;

        fd = hev_task_io_socket_accept (base->fd, NULL, NULL, NULL, NULL);
        if (fd < 0)
            continue;

        klass->dispatch (self, fd);
    }

exit:
    hev_object_unref (HEV_OBJECT (base));
}

static void
hev_fsh_client_listen_run (HevFshIO *base)
{
    LOG_D ("%p fsh client listen run", base);

    hev_task_run (base->task, hev_fsh_client_listen_task_entry, base);
}

HevFshClientBase *
hev_fsh_client_listen_new (HevFshConfig *config)
{
    HevFshClientListen *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClientListen));
    if (!self)
        return NULL;

    res = hev_fsh_client_listen_construct (self, config);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client listen new", self);

    return HEV_FSH_CLIENT_BASE (self);
}

int
hev_fsh_client_listen_construct (HevFshClientListen *self, HevFshConfig *config)
{
    int res;

    res = hev_fsh_client_base_construct (&self->base, config);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client listen construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_LISTEN_TYPE;

    return 0;
}

static void
hev_fsh_client_listen_destruct (HevObject *base)
{
    HevFshClientListen *self = HEV_FSH_CLIENT_LISTEN (base);

    LOG_D ("%p fsh client listen destruct", self);

    HEV_FSH_CLIENT_BASE_TYPE->destruct (base);
}

HevObjectClass *
hev_fsh_client_listen_class (void)
{
    static HevFshClientListenClass klass;
    HevFshClientListenClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshIOClass *ikptr;

        memcpy (kptr, HEV_FSH_CLIENT_BASE_TYPE, sizeof (HevFshClientBaseClass));

        okptr->name = "HevFshClientListen";
        okptr->destruct = hev_fsh_client_listen_destruct;

        ikptr = HEV_FSH_IO_CLASS (kptr);
        ikptr->run = hev_fsh_client_listen_run;
    }

    return okptr;
}
