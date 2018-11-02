/*
 ============================================================================
 Name        : hev-fsh-client-term-accept.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client term accept
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_TERM_ACCEPT_H__
#define __HEV_FSH_CLIENT_TERM_ACCEPT_H__

#include "hev-fsh-config.h"
#include "hev-fsh-protocol.h"
#include "hev-fsh-client-base.h"

typedef struct _HevFshClientTermAccept HevFshClientTermAccept;

HevFshClientTermAccept *hev_fsh_client_term_accept_new (HevFshConfig *config,
                                                        HevFshToken token);

#endif /* __HEV_FSH_CLIENT_TERM_ACCEPT_H__ */
