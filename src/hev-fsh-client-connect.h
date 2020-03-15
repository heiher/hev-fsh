/*
 ============================================================================
 Name        : hev-fsh-client-connect.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client connect
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_CONNECT_H__
#define __HEV_FSH_CLIENT_CONNECT_H__

#include "hev-fsh-config.h"
#include "hev-fsh-client-base.h"
#include "hev-fsh-session-manager.h"

#define HEV_FSH_CLIENT_CONNECT(p) ((HevFshClientConnect *)p)

typedef struct _HevFshClientConnect HevFshClientConnect;
typedef void (*HevFshClientConnectDestroy) (HevFshClientConnect *self);

struct _HevFshClientConnect
{
    HevFshClientBase base;

    /* private */
    HevFshClientConnectDestroy _destroy;
};

int hev_fsh_client_connect_construct (HevFshClientConnect *self,
                                      HevFshConfig *config,
                                      HevFshSessionManager *sm);

int hev_fsh_client_connect_send_connect (HevFshClientConnect *self);

#endif /* __HEV_FSH_CLIENT_CONNECT_H__ */
