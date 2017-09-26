/*
 ============================================================================
 Name        : hev-fsh-client.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 everyone.
 Description : Fsh client
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_H__
#define __HEV_FSH_CLIENT_H__

typedef struct _HevFshClient HevFshClient;

HevFshClient * hev_fsh_client_new_forward (const char *address, unsigned int port,
			const char *user);
HevFshClient * hev_fsh_client_new_connect (const char *address, unsigned int port,
			const char *token);
void hev_fsh_client_destroy (HevFshClient *self);

#endif /* __HEV_FSH_CLIENT_H__ */

