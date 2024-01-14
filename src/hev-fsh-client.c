/*
 ============================================================================
 Name        : hev-fsh-client.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2020 - 2023 xyz
 Description : Fsh client
 ============================================================================
 */

#include <stdlib.h>
#include <string.h>

#include <hev-memory-allocator.h>

#include "hev-logger.h"

#include "hev-fsh-client.h"

void
hev_fsh_client_start (HevFshBase *base)
{
    HevFshClient *self = HEV_FSH_CLIENT (base);
    HevFshClientBase *client;

    LOG_D ("%p fsh client start", base);

    client = hev_fsh_client_factory_get (self->factory);
    hev_fsh_io_run (HEV_FSH_IO (client));
}

void
hev_fsh_client_stop (HevFshBase *base)
{
    LOG_D ("%p fsh clinet stop", base);

    exit (0);
}

void
hev_fsh_client_reload (HevFshBase *base)
{
    LOG_D ("%p fsh clinet reload", base);
}

HevFshBase *
hev_fsh_client_new (HevFshConfig *config)
{
    HevFshClient *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClient));
    if (!self)
        return NULL;

    res = hev_fsh_client_construct (self, config);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client new", self);

    return HEV_FSH_BASE (self);
}

int
hev_fsh_client_construct (HevFshClient *self, HevFshConfig *config)
{
    int res;

    res = hev_fsh_base_construct (&self->base);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_TYPE;

    self->factory = hev_fsh_client_factory_new (config);
    if (!self->factory) {
        LOG_E ("%p fsh client factory", self);
        return -1;
    }

    return 0;
}

static void
hev_fsh_client_destruct (HevObject *base)
{
    HevFshClient *self = HEV_FSH_CLIENT (base);

    LOG_D ("%p fsh client destruct", self);

    hev_object_unref (HEV_OBJECT (self->factory));

    HEV_FSH_BASE_TYPE->destruct (base);
}

HevObjectClass *
hev_fsh_client_class (void)
{
    static HevFshClientClass klass;
    HevFshClientClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshBaseClass *bkptr;

        memcpy (kptr, HEV_FSH_BASE_TYPE, sizeof (HevFshBaseClass));

        okptr->name = "HevFshClient";
        okptr->destruct = hev_fsh_client_destruct;

        bkptr = HEV_FSH_BASE_CLASS (kptr);
        bkptr->start = hev_fsh_client_start;
        bkptr->stop = hev_fsh_client_stop;
        bkptr->reload = hev_fsh_client_reload;
    }

    return okptr;
}
