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

#include <netinet/in.h>

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

    /* private */
    HevFshSessionManager *_sm;
    HevFshClientBaseDestroy _destroy;
};

int hev_fsh_client_base_construct (HevFshClientBase *self, HevFshConfig *config,
                                   HevFshSessionManager *sm);
void hev_fsh_client_base_destroy (HevFshClientBase *self);

int hev_fsh_client_base_resolv (HevFshClientBase *self, struct sockaddr *addr);

#endif /* __HEV_FSH_CLIENT_BASE_H__ */
