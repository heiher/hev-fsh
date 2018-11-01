/*
 ============================================================================
 Name        : hev-fsh-client-port-listen.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client port listen
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_PORT_LISTEN_H__
#define __HEV_FSH_CLIENT_PORT_LISTEN_H__

#include "hev-fsh-client-base.h"

typedef struct _HevFshClientPortListen HevFshClientPortListen;

HevFshClientPortListen *hev_fsh_client_port_listen_new (const char *address,
                                                        unsigned int port,
                                                        const char *srv_addr,
                                                        unsigned int srv_port,
                                                        const char *token);

#endif /* __HEV_FSH_CLIENT_PORT_LISTEN_H__ */
