/*
 ============================================================================
 Name        : hev-fsh-client-connect.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client connect
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_CONNECT_H__
#define __HEV_FSH_CLIENT_CONNECT_H__

#include "hev-task.h"
#include "hev-fsh-config.h"
#include "hev-fsh-client-base.h"

typedef struct _HevFshClientConnect HevFshClientConnect;
typedef void (*HevFshClientConnectDestroy) (HevFshClientConnect *self);

struct _HevFshClientConnect
{
    HevFshClientBase base;

    HevTask *task;
    HevFshConfig *config;

    /* private */
    HevFshClientConnectDestroy _destroy;
};

int hev_fsh_client_connect_construct (HevFshClientConnect *self,
                                      HevFshConfig *config);

int hev_fsh_client_connect_send_connect (HevFshClientConnect *self);

#endif /* __HEV_FSH_CLIENT_CONNECT_H__ */
