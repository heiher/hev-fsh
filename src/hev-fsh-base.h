/*
 ============================================================================
 Name        : hev-fsh-base.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2020 - 2021 xyz
 Description : Fsh base
 ============================================================================
 */

#ifndef __HEV_FSH_BASE_H__
#define __HEV_FSH_BASE_H__

#include "hev-object.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_BASE(p) ((HevFshBase *)p)
#define HEV_FSH_BASE_CLASS(p) ((HevFshBaseClass *)p)
#define HEV_FSH_BASE_TYPE (hev_fsh_base_class ())

typedef struct _HevFshBase HevFshBase;
typedef struct _HevFshBaseClass HevFshBaseClass;

struct _HevFshBase
{
    HevObject base;
};

struct _HevFshBaseClass
{
    HevObjectClass base;

    void (*start) (HevFshBase *self);
    void (*stop) (HevFshBase *self);
};

HevObjectClass *hev_fsh_base_class (void);

int hev_fsh_base_construct (HevFshBase *self);

void hev_fsh_base_start (HevFshBase *self);
void hev_fsh_base_stop (HevFshBase *self);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_BASE_H__ */
