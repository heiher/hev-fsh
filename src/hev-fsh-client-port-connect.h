/*
 ============================================================================
 Name        : hev-fsh-client-port-connect.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client port connect
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_PORT_CONNECT_H__
#define __HEV_FSH_CLIENT_PORT_CONNECT_H__

#include "hev-fsh-client-base.h"

typedef struct _HevFshClientPortConnect HevFshClientPortConnect;

HevFshClientPortConnect *hev_fsh_client_port_connect_new (const char *address,
                                                          unsigned int port,
                                                          const char *token,
                                                          int local_fd);

#endif /* __HEV_FSH_CLIENT_PORT_CONNECT_H__ */
