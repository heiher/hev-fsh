/*
 ============================================================================
 Name        : hev-fsh-client-port-listen.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client port listen
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_PORT_LISTEN_H__
#define __HEV_FSH_CLIENT_PORT_LISTEN_H__

#include "hev-fsh-config.h"
#include "hev-fsh-client-base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_PORT_LISTEN(p) ((HevFshClientPortListen *)p)
#define HEV_FSH_CLIENT_PORT_LISTEN_CLASS(P) ((HevFshClientPortListenClass *)p)
#define HEV_FSH_CLIENT_PORT_LISTEN_TYPE (hev_fsh_client_port_listen_class ())

typedef struct _HevFshClientPortListen HevFshClientPortListen;
typedef struct _HevFshClientPortListenClass HevFshClientPortListenClass;

struct _HevFshClientPortListen
{
    HevFshClientBase base;
};

struct _HevFshClientPortListenClass
{
    HevFshClientBaseClass base;
};

HevObjectClass *hev_fsh_client_port_listen_class (void);

int hev_fsh_client_port_listen_construct (HevFshClientPortListen *self,
                                          HevFshConfig *config);

HevFshClientBase *hev_fsh_client_port_listen_new (HevFshConfig *config);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_PORT_LISTEN_H__ */
