/*
 ============================================================================
 Name        : hev-fsh-client-term-forward.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client term forward
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_TERM_FORWARD_H__
#define __HEV_FSH_CLIENT_TERM_FORWARD_H__

#include "hev-fsh-config.h"
#include "hev-fsh-client-base.h"

typedef struct _HevFshClientTermForward HevFshClientTermForward;

HevFshClientBase *hev_fsh_client_term_forward_new (HevFshConfig *config);

#endif /* __HEV_FSH_CLIENT_TERM_FORWARD_H__ */
