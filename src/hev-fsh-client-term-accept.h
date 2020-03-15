/*
 ============================================================================
 Name        : hev-fsh-client-term-accept.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client term accept
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_TERM_ACCEPT_H__
#define __HEV_FSH_CLIENT_TERM_ACCEPT_H__

#include "hev-fsh-client-accept.h"
#include "hev-fsh-session-manager.h"

#define HEV_FSH_CLIENT_TERM_ACCEPT(p) ((HevFshClientTermAccept *)p)

typedef struct _HevFshClientTermAccept HevFshClientTermAccept;

HevFshClientBase *hev_fsh_client_term_accept_new (HevFshConfig *config,
                                                  HevFshToken token,
                                                  HevFshSessionManager *sm);

#endif /* __HEV_FSH_CLIENT_TERM_ACCEPT_H__ */
