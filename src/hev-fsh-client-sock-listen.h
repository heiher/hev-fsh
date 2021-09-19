/*
 ============================================================================
 Name        : hev-fsh-client-sock-listen.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh client sock listen
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_SOCK_LISTEN_H__
#define __HEV_FSH_CLIENT_SOCK_LISTEN_H__

#include "hev-fsh-client-listen.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_SOCK_LISTEN(p) ((HevFshClientSockListen *)p)
#define HEV_FSH_CLIENT_SOCK_LISTEN_CLASS(p) ((HevFshClientSockListenClass *)p)
#define HEV_FSH_CLIENT_SOCK_LISTEN_TYPE (hev_fsh_client_sock_listen_class ())

typedef struct _HevFshClientSockListen HevFshClientSockListen;
typedef struct _HevFshClientSockListenClass HevFshClientSockListenClass;

struct _HevFshClientSockListen
{
    HevFshClientListen base;
};

struct _HevFshClientSockListenClass
{
    HevFshClientListenClass base;
};

HevObjectClass *hev_fsh_client_sock_listen_class (void);

int hev_fsh_client_sock_listen_construct (HevFshClientSockListen *self,
                                          HevFshConfig *config);

HevFshClientBase *hev_fsh_client_sock_listen_new (HevFshConfig *config);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_SOCK_LISTEN_H__ */
