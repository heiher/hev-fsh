/*
 ============================================================================
 Name        : hev-fsh-client-term-connect.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client term connect
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_TERM_CONNECT_H__
#define __HEV_FSH_CLIENT_TERM_CONNECT_H__

#include "hev-fsh-client-connect.h"
#include "hev-fsh-session-manager.h"

typedef struct _HevFshClientTermConnect HevFshClientTermConnect;

HevFshClientBase *hev_fsh_client_term_connect_new (HevFshConfig *config,
                                                   HevFshSessionManager *sm);

#endif /* __HEV_FSH_CLIENT_TERM_CONNECT_H__ */
