/*
 ============================================================================
 Name        : hev-fsh-client-factory.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh client factory
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_FACTORY_H__
#define __HEV_FSH_CLIENT_FACTORY_H__

#include "hev-object.h"
#include "hev-fsh-config.h"
#include "hev-fsh-client-base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_FACTORY(p) ((HevFshClientFactory *)p)
#define HEV_FSH_CLIENT_FACTORY_CLASS(p) ((HevFshClientFactoryClass *)p)
#define HEV_FSH_CLIENT_FACTORY_TYPE (hev_fsh_client_factory_class ())

typedef struct _HevFshClientFactory HevFshClientFactory;
typedef struct _HevFshClientFactoryClass HevFshClientFactoryClass;

struct _HevFshClientFactory
{
    HevObject base;

    HevFshConfig *config;
};

struct _HevFshClientFactoryClass
{
    HevObjectClass base;
};

HevObjectClass *hev_fsh_client_factory_class (void);

int hev_fsh_client_factory_construct (HevFshClientFactory *self,
                                      HevFshConfig *config);

HevFshClientFactory *hev_fsh_client_factory_new (HevFshConfig *config);

HevFshClientBase *hev_fsh_client_factory_get (HevFshClientFactory *self);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_H__ */
