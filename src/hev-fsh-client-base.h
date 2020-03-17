/*
 ============================================================================
 Name        : hev-fsh-client-base.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client base
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_BASE_H__
#define __HEV_FSH_CLIENT_BASE_H__

#include "hev-fsh-config.h"
#include "hev-fsh-session.h"
#include "hev-fsh-session-manager.h"

#define HEV_FSH_CLIENT_BASE(p) ((HevFshClientBase *)p)

typedef struct _HevFshClientBase HevFshClientBase;
typedef void (*HevFshClientBaseDestroy) (HevFshClientBase *self);

struct _HevFshClientBase
{
    HevFshSession base;

    int fd;
    HevFshConfig *config;
    HevFshSessionManager *sm;

    /* private */
    HevFshClientBaseDestroy _destroy;
};

void hev_fsh_client_base_init (HevFshClientBase *self, HevFshConfig *config,
                               HevFshSessionManager *sm);
void hev_fsh_client_base_destroy (HevFshClientBase *self);

int hev_fsh_client_base_listen (HevFshClientBase *self);
int hev_fsh_client_base_connect (HevFshClientBase *self);

#endif /* __HEV_FSH_CLIENT_BASE_H__ */
