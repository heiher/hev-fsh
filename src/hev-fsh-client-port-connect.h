/*
 ============================================================================
 Name        : hev-fsh-client-port-connect.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client port connect
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_PORT_CONNECT_H__
#define __HEV_FSH_CLIENT_PORT_CONNECT_H__

#include "hev-fsh-client-connect.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_PORT_CONNECT(p) ((HevFshClientPortConnect *)p)
#define HEV_FSH_CLIENT_PORT_CONNECT_CLASS(P) ((HevFshClientPortConnectClass *)p)
#define HEV_FSH_CLIENT_PORT_CONNECT_TYPE (hev_fsh_client_port_connect_class ())

typedef struct _HevFshClientPortConnect HevFshClientPortConnect;
typedef struct _HevFshClientPortConnectClass HevFshClientPortConnectClass;

struct _HevFshClientPortConnect
{
    HevFshClientConnect base;

    int fd;
};

struct _HevFshClientPortConnectClass
{
    HevFshClientConnectClass base;
};

HevObjectClass *hev_fsh_client_port_connect_class (void);

int hev_fsh_client_port_connect_construct (HevFshClientPortConnect *self,
                                           HevFshConfig *config, int fd);

HevFshClientBase *hev_fsh_client_port_connect_new (HevFshConfig *config,
                                                   int fd);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_PORT_CONNECT_H__ */
