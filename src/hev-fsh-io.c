/*
 ============================================================================
 Name        : hev-fsh-io.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh IO
 ============================================================================
 */

#include <string.h>

#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-fsh-config.h"

#include "hev-fsh-io.h"

int
hev_fsh_io_yielder (HevTaskYieldType type, void *data)
{
    HevFshIO *self = data;

    if (type == HEV_TASK_YIELD) {
        hev_task_yield (HEV_TASK_YIELD);
        return 0;
    }

    if (self->timeout < 0) {
        hev_task_yield (HEV_TASK_WAITIO);
    } else {
        unsigned int timeout = self->timeout;
        timeout = hev_task_sleep (timeout);
        if (timeout == 0) {
            LOG_D ("%p fsh io timeout", self);
            return -1;
        }
    }

    return 0;
}

void
hev_fsh_io_run (HevFshIO *self)
{
    HevFshIOClass *klass = HEV_OBJECT_GET_CLASS (self);

    LOG_D ("%p fsh io run", self);

    klass->run (self);
}

int
hev_fsh_io_construct (HevFshIO *self, unsigned int timeout)
{
    int res;

    res = hev_object_construct (&self->base);
    if (res < 0)
        return res;

    LOG_D ("%p fsh io construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_IO_TYPE;

    self->task = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!self->task)
        return -1;

    self->timeout = timeout * 1000;

    return 0;
}

static void
hev_fsh_io_destruct (HevObject *base)
{
    HevFshIO *self = HEV_FSH_IO (base);

    LOG_D ("%p fsh io destruct", self);

    HEV_OBJECT_TYPE->finalizer (base);
    hev_free (self);
}

HevObjectClass *
hev_fsh_io_class (void)
{
    static HevFshIOClass klass;
    HevFshIOClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        memcpy (kptr, HEV_OBJECT_TYPE, sizeof (HevObjectClass));

        okptr->name = "HevFshIO";
        okptr->finalizer = hev_fsh_io_destruct;
    }

    return okptr;
}
