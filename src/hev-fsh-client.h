/*
 ============================================================================
 Name        : hev-fsh-client.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2020 - 2021 xyz
 Description : Fsh client
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_H__
#define __HEV_FSH_CLIENT_H__

#include "hev-fsh-base.h"
#include "hev-fsh-config.h"
#include "hev-fsh-client-factory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT(p) ((HevFshClient *)p)
#define HEV_FSH_CLIENT_CLASS(p) ((HevFshClientClass *)p)
#define HEV_FSH_CLIENT_TYPE (hev_fsh_client_class ())

typedef struct _HevFshClient HevFshClient;
typedef struct _HevFshClientClass HevFshClientClass;

struct _HevFshClient
{
    HevFshBase base;

    HevFshClientFactory *factory;
};

struct _HevFshClientClass
{
    HevFshBaseClass base;
};

HevObjectClass *hev_fsh_client_class (void);

int hev_fsh_client_construct (HevFshClient *self, HevFshConfig *config);

HevFshBase *hev_fsh_client_new (HevFshConfig *config);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_H__ */
