/*
 ============================================================================
 Name        : hev-fsh-client-base.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client base
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_BASE_H__
#define __HEV_FSH_CLIENT_BASE_H__

#include "hev-fsh-io.h"
#include "hev-fsh-config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_BASE(p) ((HevFshClientBase *)p)
#define HEV_FSH_CLIENT_BASE_CLASS(p) ((HevFshClientBaseClass *)p)
#define HEV_FSH_CLIENT_BASE_TYPE (hev_fsh_client_base_class ())

typedef struct _HevFshClientBase HevFshClientBase;
typedef struct _HevFshClientBaseClass HevFshClientBaseClass;

struct _HevFshClientBase
{
    HevFshIO base;

    int fd;
    HevFshConfig *config;
};

struct _HevFshClientBaseClass
{
    HevFshIOClass base;
};

HevObjectClass *hev_fsh_client_base_class (void);

int hev_fsh_client_base_construct (HevFshClientBase *self,
                                   HevFshConfig *config);

int hev_fsh_client_base_listen (HevFshClientBase *self);
int hev_fsh_client_base_connect (HevFshClientBase *self);
int hev_fsh_client_base_encrypt (HevFshClientBase *self);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_BASE_H__ */
