/*
 ============================================================================
 Name        : hev-fsh-client-sock-accept.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh client sock accept
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_SOCK_ACCEPT_H__
#define __HEV_FSH_CLIENT_SOCK_ACCEPT_H__

#include "hev-fsh-client-accept.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_SOCK_ACCEPT(p) ((HevFshClientSockAccept *)p)
#define HEV_FSH_CLIENT_SOCK_ACCEPT_CLASS(P) ((HevFshClientSockAcceptClass *)p)
#define HEV_FSH_CLIENT_SOCK_ACCEPT_TYPE (hev_fsh_client_sock_accept_class ())

typedef struct _HevFshClientSockAccept HevFshClientSockAccept;
typedef struct _HevFshClientSockAcceptClass HevFshClientSockAcceptClass;

struct _HevFshClientSockAccept
{
    HevFshClientAccept base;
};

struct _HevFshClientSockAcceptClass
{
    HevFshClientAcceptClass base;
};

HevObjectClass *hev_fsh_client_sock_accept_class (void);

int hev_fsh_client_sock_accept_construct (HevFshClientSockAccept *self,
                                          HevFshConfig *config,
                                          HevFshToken token);

HevFshClientBase *hev_fsh_client_sock_accept_new (HevFshConfig *config,
                                                  HevFshToken token);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_SOCK_ACCEPT_H__ */
