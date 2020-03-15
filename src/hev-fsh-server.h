/*
 ============================================================================
 Name        : hev-fsh-server.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2020 everyone.
 Description : Fsh server
 ============================================================================
 */

#ifndef __HEV_FSH_SERVER_H__
#define __HEV_FSH_SERVER_H__

#include "hev-fsh-base.h"
#include "hev-fsh-config.h"

#define HEV_FSH_SERVER(p) ((HevFshServer *)p)

typedef struct _HevFshServer HevFshServer;

HevFshBase *hev_fsh_server_new (HevFshConfig *config);

#endif /* __HEV_FSH_SERVER_H__ */
