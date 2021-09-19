/*
 ============================================================================
 Name        : hev-fsh-client-sock-connect.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh client sock connect
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_SOCK_CONNECT_H__
#define __HEV_FSH_CLIENT_SOCK_CONNECT_H__

#include "hev-fsh-client-connect.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_SOCK_CONNECT(p) ((HevFshClientSockConnect *)p)
#define HEV_FSH_CLIENT_SOCK_CONNECT_CLASS(P) ((HevFshClientSockConnectClass *)p)
#define HEV_FSH_CLIENT_SOCK_CONNECT_TYPE (hev_fsh_client_sock_connect_class ())

typedef struct _HevFshClientSockConnect HevFshClientSockConnect;
typedef struct _HevFshClientSockConnectClass HevFshClientSockConnectClass;

struct _HevFshClientSockConnect
{
    HevFshClientConnect base;

    int fd;
};

struct _HevFshClientSockConnectClass
{
    HevFshClientConnectClass base;
};

HevObjectClass *hev_fsh_client_sock_connect_class (void);

int hev_fsh_client_sock_connect_construct (HevFshClientSockConnect *self,
                                           HevFshConfig *config, int fd);

HevFshClientBase *hev_fsh_client_sock_connect_new (HevFshConfig *config,
                                                   int fd);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_SOCK_CONNECT_H__ */
