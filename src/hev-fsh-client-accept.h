/*
 ============================================================================
 Name        : hev-fsh-client-accept.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2019 everyone.
 Description : Fsh client accept
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_ACCEPT_H__
#define __HEV_FSH_CLIENT_ACCEPT_H__

#include "hev-task.h"
#include "hev-fsh-config.h"
#include "hev-fsh-protocol.h"
#include "hev-fsh-client-base.h"

typedef struct _HevFshClientAccept HevFshClientAccept;
typedef void (*HevFshClientAcceptDestroy) (HevFshClientAccept *self);

struct _HevFshClientAccept
{
    HevFshClientBase base;

    HevFshToken token;

    HevTask *task;
    HevFshConfig *config;

    /* private */
    HevFshClientAcceptDestroy _destroy;
};

int hev_fsh_client_accept_construct (HevFshClientAccept *self,
                                     HevFshConfig *config, HevFshToken token);

int hev_fsh_client_accept_send_accept (HevFshClientAccept *self);

#endif /* __HEV_FSH_CLIENT_ACCEPT_H__ */
