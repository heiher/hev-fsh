/*
 ============================================================================
 Name        : hev-fsh-client-port-accept.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client port accept
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_PORT_ACCEPT_H__
#define __HEV_FSH_CLIENT_PORT_ACCEPT_H__

#include "hev-fsh-protocol.h"
#include "hev-fsh-client-base.h"

typedef struct _HevFshClientPortAccept HevFshClientPortAccept;

HevFshClientPortAccept *hev_fsh_client_port_accept_new (struct sockaddr *addr,
                                                        socklen_t addrlen,
                                                        const char *address,
                                                        unsigned int port,
                                                        HevFshToken *token);

#endif /* __HEV_FSH_CLIENT_PORT_ACCEPT_H__ */
