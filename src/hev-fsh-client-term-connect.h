/*
 ============================================================================
 Name        : hev-fsh-client-term-connect.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client term connect
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_TERM_CONNECT_H__
#define __HEV_FSH_CLIENT_TERM_CONNECT_H__

#include "hev-fsh-client-connect.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_TERM_CONNECT(p) ((HevFshClientTermConnect *)p)
#define HEV_FSH_CLIENT_TERM_CONNECT_CLASS(P) ((HevFshClientTermConnectClass *)p)
#define HEV_FSH_CLIENT_TERM_CONNECT_TYPE (hev_fsh_client_term_connect_class ())

typedef struct _HevFshClientTermConnect HevFshClientTermConnect;
typedef struct _HevFshClientTermConnectClass HevFshClientTermConnectClass;

struct _HevFshClientTermConnect
{
    HevFshClientConnect base;
};

struct _HevFshClientTermConnectClass
{
    HevFshClientConnectClass base;
};

HevObjectClass *hev_fsh_client_term_connect_class (void);

int hev_fsh_client_term_connect_construct (HevFshClientTermConnect *self,
                                           HevFshConfig *config);

HevFshClientBase *hev_fsh_client_term_connect_new (HevFshConfig *config);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_TERM_CONNECT_H__ */
