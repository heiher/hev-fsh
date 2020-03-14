/*
 ============================================================================
 Name        : hev-fsh-base.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2020 everyone.
 Description : Fsh base
 ============================================================================
 */

#ifndef __HEV_FSH_BASE_H__
#define __HEV_FSH_BASE_H__

typedef struct _HevFshBase HevFshBase;
typedef void (*HevFshBaseStart) (HevFshBase *self);
typedef void (*HevFshBaseStop) (HevFshBase *self);
typedef void (*HevFshBaseDestroy) (HevFshBase *self);

struct _HevFshBase
{
    /* private */
    HevFshBaseStart _start;
    HevFshBaseStop _stop;
    HevFshBaseDestroy _destroy;
};

void hev_fsh_base_destroy (HevFshBase *self);

void hev_fsh_base_start (HevFshBase *self);
void hev_fsh_base_stop (HevFshBase *self);

#endif /* __HEV_FSH_BASE_H__ */
