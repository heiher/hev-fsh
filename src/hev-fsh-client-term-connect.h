/*
 ============================================================================
 Name        : hev-fsh-client-term-connect.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client term connect
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_TERM_CONNECT_H__
#define __HEV_FSH_CLIENT_TERM_CONNECT_H__

#include "hev-fsh-config.h"
#include "hev-fsh-client-base.h"

typedef struct _HevFshClientTermConnect HevFshClientTermConnect;

HevFshClientTermConnect *hev_fsh_client_term_connect_new (HevFshConfig *config);

#endif /* __HEV_FSH_CLIENT_TERM_CONNECT_H__ */
