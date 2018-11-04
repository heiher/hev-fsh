/*
 ============================================================================
 Name        : hev-fsh-client-forward.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh client forward
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_FORWARD_H__
#define __HEV_FSH_CLIENT_FORWARD_H__

#include "hev-fsh-config.h"
#include "hev-fsh-client-base.h"

typedef struct _HevFshClientForward HevFshClientForward;

HevFshClientBase *hev_fsh_client_forward_new (HevFshConfig *config);

#endif /* __HEV_FSH_CLIENT_FORWARD_H__ */
