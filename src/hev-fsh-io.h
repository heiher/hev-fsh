/*
 ============================================================================
 Name        : hev-fsh-io.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh IO
 ============================================================================
 */

#ifndef __HEV_FSH_IO_H__
#define __HEV_FSH_IO_H__

#include <hev-task.h>
#include <hev-task-io.h>

#include "hev-object.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_IO(p) ((HevFshIO *)p)
#define HEV_FSH_IO_CLASS(p) ((HevFshIOClass *)p)
#define HEV_FSH_IO_TYPE (hev_fsh_io_class ())
#define io_yielder hev_fsh_io_yielder

typedef struct _HevFshIO HevFshIO;
typedef struct _HevFshIOClass HevFshIOClass;

struct _HevFshIO
{
    HevObject base;

    HevTask *task;
    unsigned int timeout;
};

struct _HevFshIOClass
{
    HevObjectClass base;

    void (*run) (HevFshIO *self);
};

HevObjectClass *hev_fsh_io_class (void);

int hev_fsh_io_construct (HevFshIO *self, unsigned int timeout);

void hev_fsh_io_run (HevFshIO *self);

int hev_fsh_io_yielder (HevTaskYieldType type, void *data);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_IO_H__ */
