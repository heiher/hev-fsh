/*
 ============================================================================
 Name        : hev-fsh-client-connect.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client connect
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_CONNECT_H__
#define __HEV_FSH_CLIENT_CONNECT_H__

#include "hev-fsh-config.h"
#include "hev-fsh-client-base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_CONNECT(p) ((HevFshClientConnect *)p)
#define HEV_FSH_CLIENT_CONNECT_CLASS(P) ((HevFshClientConnectClass *)p)
#define HEV_FSH_CLIENT_CONNECT_TYPE (hev_fsh_client_connect_class ())

typedef struct _HevFshClientConnect HevFshClientConnect;
typedef struct _HevFshClientConnectClass HevFshClientConnectClass;

struct _HevFshClientConnect
{
    HevFshClientBase base;
};

struct _HevFshClientConnectClass
{
    HevFshClientBaseClass base;
};

HevObjectClass *hev_fsh_client_connect_class (void);

int hev_fsh_client_connect_construct (HevFshClientConnect *self,
                                      HevFshConfig *config);

int hev_fsh_client_connect_send_connect (HevFshClientConnect *self);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_CONNECT_H__ */
