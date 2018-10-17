/*
 ============================================================================
 Name        : hev-fsh-server.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 everyone.
 Description : Fsh server
 ============================================================================
 */

#ifndef __HEV_FSH_SERVER_H__
#define __HEV_FSH_SERVER_H__

typedef struct _HevFshServer HevFshServer;

HevFshServer *hev_fsh_server_new (const char *address, unsigned int port);
void hev_fsh_server_destroy (HevFshServer *self);

void hev_fsh_server_start (HevFshServer *self);
void hev_fsh_server_stop (HevFshServer *self);

#endif /* __HEV_FSH_SERVER_H__ */
