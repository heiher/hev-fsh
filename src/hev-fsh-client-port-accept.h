/*
 ============================================================================
 Name        : hev-fsh-client-port-accept.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client port accept
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_PORT_ACCEPT_H__
#define __HEV_FSH_CLIENT_PORT_ACCEPT_H__

#include "hev-fsh-client-accept.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_PORT_ACCEPT(p) ((HevFshClientPortAccept *)p)
#define HEV_FSH_CLIENT_PORT_ACCEPT_CLASS(P) ((HevFshClientPortAcceptClass *)p)
#define HEV_FSH_CLIENT_PORT_ACCEPT_TYPE (hev_fsh_client_port_accept_class ())

typedef struct _HevFshClientPortAccept HevFshClientPortAccept;
typedef struct _HevFshClientPortAcceptClass HevFshClientPortAcceptClass;

struct _HevFshClientPortAccept
{
    HevFshClientAccept base;
};

struct _HevFshClientPortAcceptClass
{
    HevFshClientAcceptClass base;
};

HevObjectClass *hev_fsh_client_port_accept_class (void);

int hev_fsh_client_port_accept_construct (HevFshClientPortAccept *self,
                                          HevFshConfig *config,
                                          HevFshToken token);

HevFshClientBase *hev_fsh_client_port_accept_new (HevFshConfig *config,
                                                  HevFshToken token);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_PORT_ACCEPT_H__ */
