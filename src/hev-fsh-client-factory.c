/*
 ============================================================================
 Name        : hev-fsh-client-factory.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh client factory
 ============================================================================
 */

#include <string.h>

#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-fsh-client-forward.h"
#include "hev-fsh-client-port-listen.h"
#include "hev-fsh-client-port-connect.h"
#include "hev-fsh-client-sock-listen.h"
#include "hev-fsh-client-term-connect.h"

#include "hev-fsh-client-factory.h"

HevFshClientBase *
hev_fsh_client_factory_get (HevFshClientFactory *self)
{
    int mode;

    LOG_D ("%p fsh client factory get", self);

    mode = hev_fsh_config_get_mode (self->config);

    if (HEV_FSH_CONFIG_MODE_FORWARDER & mode) {
        return hev_fsh_client_forward_new (self->config);
    } else if (HEV_FSH_CONFIG_MODE_CONNECTOR_PORT == mode) {
        if (hev_fsh_config_get_local_port (self->config))
            return hev_fsh_client_port_listen_new (self->config);
        else
            return hev_fsh_client_port_connect_new (self->config, -1);
    } else if (HEV_FSH_CONFIG_MODE_CONNECTOR_SOCK == mode) {
        return hev_fsh_client_sock_listen_new (self->config);
    } else if (HEV_FSH_CONFIG_MODE_CONNECTOR_TERM == mode) {
        return hev_fsh_client_term_connect_new (self->config);
    }

    return NULL;
}

HevFshClientFactory *
hev_fsh_client_factory_new (HevFshConfig *config)
{
    HevFshClientFactory *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClientFactory));
    if (!self)
        return NULL;

    res = hev_fsh_client_factory_construct (self, config);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client factory new", self);

    return self;
}

int
hev_fsh_client_factory_construct (HevFshClientFactory *self,
                                  HevFshConfig *config)
{
    int res;

    res = hev_object_construct (&self->base);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client factory construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_FACTORY_TYPE;

    self->config = config;

    return 0;
}

static void
hev_fsh_client_factory_destruct (HevObject *base)
{
    HevFshClientFactory *self = HEV_FSH_CLIENT_FACTORY (base);

    LOG_D ("%p fsh client factory destruct", self);

    HEV_OBJECT_TYPE->destruct (base);
    hev_free (self);
}

HevObjectClass *
hev_fsh_client_factory_class (void)
{
    static HevFshClientFactoryClass klass;
    HevFshClientFactoryClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        memcpy (kptr, HEV_OBJECT_TYPE, sizeof (HevObjectClass));

        okptr->name = "HevFshClientFactory";
        okptr->destruct = hev_fsh_client_factory_destruct;
    }

    return okptr;
}
