/*
 ============================================================================
 Name        : hev-fsh-base.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2020 - 2023 xyz
 Description : Fsh base
 ============================================================================
 */

#include <string.h>

#include <hev-memory-allocator.h>

#include "hev-logger.h"

#include "hev-fsh-base.h"

void
hev_fsh_base_start (HevFshBase *self)
{
    HevFshBaseClass *klass = HEV_OBJECT_GET_CLASS (self);

    LOG_D ("%p fsh base start", self);

    klass->start (self);
}

void
hev_fsh_base_stop (HevFshBase *self)
{
    HevFshBaseClass *klass = HEV_OBJECT_GET_CLASS (self);

    LOG_D ("%p fsh base stop", self);

    klass->stop (self);
}

void
hev_fsh_base_reload (HevFshBase *self)
{
    HevFshBaseClass *klass = HEV_OBJECT_GET_CLASS (self);

    LOG_D ("%p fsh base reload", self);

    klass->reload (self);
}

int
hev_fsh_base_construct (HevFshBase *self)
{
    int res;

    res = hev_object_construct (&self->base);
    if (res < 0)
        return res;

    LOG_D ("%p fsh base construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_BASE_TYPE;

    return 0;
}

static void
hev_fsh_base_destruct (HevObject *base)
{
    HevFshBase *self = HEV_FSH_BASE (base);

    LOG_D ("%p fsh base destruct", self);

    HEV_OBJECT_TYPE->destruct (base);
    hev_free (base);
}

HevObjectClass *
hev_fsh_base_class (void)
{
    static HevFshBaseClass klass;
    HevFshBaseClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        memcpy (kptr, HEV_OBJECT_TYPE, sizeof (HevObjectClass));

        okptr->name = "HevFshBase";
        okptr->destruct = hev_fsh_base_destruct;
    }

    return okptr;
}
