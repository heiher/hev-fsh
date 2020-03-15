/*
 ============================================================================
 Name        : hev-fsh-client-port-listen.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client port listen
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_PORT_LISTEN_H__
#define __HEV_FSH_CLIENT_PORT_LISTEN_H__

#include "hev-fsh-config.h"
#include "hev-fsh-client-base.h"
#include "hev-fsh-session-manager.h"

#define HEV_FSH_CLIENT_PORT_LISTEN(p) ((HevFshClientPortListen *)p)

typedef struct _HevFshClientPortListen HevFshClientPortListen;

HevFshClientBase *hev_fsh_client_port_listen_new (HevFshConfig *config,
                                                  HevFshSessionManager *sm);

#endif /* __HEV_FSH_CLIENT_PORT_LISTEN_H__ */
