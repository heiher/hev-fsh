/*
 ============================================================================
 Name        : hev-fsh-client-accept.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client accept
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_ACCEPT_H__
#define __HEV_FSH_CLIENT_ACCEPT_H__

#include "hev-fsh-config.h"
#include "hev-fsh-protocol.h"
#include "hev-fsh-client-base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_ACCEPT(p) ((HevFshClientAccept *)p)
#define HEV_FSH_CLIENT_ACCEPT_CLASS(P) ((HevFshClientAcceptClass *)p)
#define HEV_FSH_CLIENT_ACCEPT_TYPE (hev_fsh_client_accept_class ())

typedef struct _HevFshClientAccept HevFshClientAccept;
typedef struct _HevFshClientAcceptClass HevFshClientAcceptClass;

struct _HevFshClientAccept
{
    HevFshClientBase base;

    HevFshToken token;
};

struct _HevFshClientAcceptClass
{
    HevFshClientBaseClass base;
};

HevObjectClass *hev_fsh_client_accept_class (void);

int hev_fsh_client_accept_construct (HevFshClientAccept *self,
                                     HevFshConfig *config, HevFshToken token);

int hev_fsh_client_accept_send_accept (HevFshClientAccept *self);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_ACCEPT_H__ */
