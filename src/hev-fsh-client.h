/*
 ============================================================================
 Name        : hev-fsh-client.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2020 everyone.
 Description : Fsh client
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_H__
#define __HEV_FSH_CLIENT_H__

#include "hev-fsh-base.h"
#include "hev-fsh-config.h"

typedef struct _HevFshClient HevFshClient;

HevFshBase *hev_fsh_client_new (HevFshConfig *config);

#endif /* __HEV_FSH_CLIENT_H__ */
