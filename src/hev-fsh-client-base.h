/*
 ============================================================================
 Name        : hev-fsh-client-base.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh client base
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_BASE_H__
#define __HEV_FSH_CLIENT_BASE_H__

#include <sys/types.h>
#include <sys/socket.h>

#include "hev-fsh-session.h"
#include "hev-fsh-session-manager.h"

typedef struct _HevFshClientBase HevFshClientBase;
typedef void (*HevFshClientBaseDestroy) (HevFshClientBase *self);

struct _HevFshClientBase
{
    HevFshSession base;

    int fd;
    struct sockaddr address;

    /* private */
    HevFshSessionManager *_sm;
    HevFshClientBaseDestroy _destroy;
};

int hev_fsh_client_base_construct (HevFshClientBase *self, const char *address,
                                   unsigned int port, HevFshSessionManager *sm);
void hev_fsh_client_base_destroy (HevFshClientBase *self);

#endif /* __HEV_FSH_CLIENT_BASE_H__ */
