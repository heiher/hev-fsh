/*
 ============================================================================
 Name        : hev-fsh-client-port-forward.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client port forward
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_PORT_FORWARD_H__
#define __HEV_FSH_CLIENT_PORT_FORWARD_H__

#include "hev-fsh-client-base.h"

typedef struct _HevFshClientPortForward HevFshClientPortForward;

HevFshClientPortForward *hev_fsh_client_port_forward_new (const char *address,
                                                          unsigned int port,
                                                          const char *srv_addr,
                                                          unsigned int srv_port,
                                                          const char *token);

#endif /* __HEV_FSH_CLIENT_PORT_FORWARD_H__ */
