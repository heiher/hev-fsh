/*
 ============================================================================
 Name        : hev-fsh-client.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2020 everyone.
 Description : Fsh client
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>

#include <hev-memory-allocator.h>

#include "hev-fsh-client-forward.h"
#include "hev-fsh-client-term-connect.h"
#include "hev-fsh-client-port-connect.h"
#include "hev-fsh-client-port-listen.h"
#include "hev-fsh-session-manager.h"

#include "hev-fsh-client.h"

struct _HevFshClient
{
    HevFshBase base;

    HevFshConfig *config;
    HevFshSessionManager *session_manager;
};

static void hev_fsh_client_destroy (HevFshBase *base);
static void hev_fsh_client_start (HevFshBase *base);
static void hev_fsh_client_stop (HevFshBase *base);
static void fsh_session_close_handler (HevFshSession *s, void *data);

HevFshBase *
hev_fsh_client_new (HevFshConfig *config)
{
    HevFshClient *self;
    HevFshSessionManager *sm;
    HevFshClientBase *cli = NULL;
    int autostop = 1;
    int timeout;
    int mode;

    self = hev_malloc0 (sizeof (HevFshClient));
    if (!self) {
        fprintf (stderr, "Allocate client failed!\n");
        goto exit;
    }

    mode = hev_fsh_config_get_mode (config);
    timeout = hev_fsh_config_get_timeout (config);

    if (HEV_FSH_CONFIG_MODE_FORWARDER & mode)
        autostop = 0;

    sm = hev_fsh_session_manager_new (timeout, autostop);
    if (!sm) {
        fprintf (stderr, "Create session manager failed!\n");
        goto exit_free;
    }

    if (HEV_FSH_CONFIG_MODE_FORWARDER & mode) {
        cli = hev_fsh_client_forward_new (config, sm, fsh_session_close_handler,
                                          self);
    } else if (HEV_FSH_CONFIG_MODE_CONNECTOR_PORT == mode) {
        if (hev_fsh_config_get_local_port (config))
            cli = hev_fsh_client_port_listen_new (config, sm);
        else
            cli = hev_fsh_client_port_connect_new (config, -1, sm);
    } else if (HEV_FSH_CONFIG_MODE_CONNECTOR_TERM == mode) {
        cli = hev_fsh_client_term_connect_new (config, sm);
    }

    if (!cli) {
        fprintf (stderr, "Create fsh client failed!\n");
        goto exit_free_sm;
    }

    self->config = config;
    self->session_manager = sm;
    self->base._destroy = hev_fsh_client_destroy;
    self->base._start = hev_fsh_client_start;
    self->base._stop = hev_fsh_client_stop;
    return &self->base;

exit_free_sm:
    hev_fsh_session_manager_destroy (sm);
exit_free:
    hev_free (self);
exit:
    return NULL;
}

static void
hev_fsh_client_destroy (HevFshBase *base)
{
    HevFshClient *self = HEV_FSH_CLIENT (base);

    hev_fsh_session_manager_destroy (self->session_manager);
    hev_free (self);
}

static void
hev_fsh_client_start (HevFshBase *base)
{
    HevFshClient *self = HEV_FSH_CLIENT (base);

    hev_fsh_session_manager_start (self->session_manager);
}

static void
hev_fsh_client_stop (HevFshBase *base)
{
    exit (0);
}

static void
fsh_session_close_handler (HevFshSession *s, void *data)
{
    HevFshClient *self = HEV_FSH_CLIENT (data);

    while (!hev_fsh_client_forward_new (self->config, self->session_manager,
                                        fsh_session_close_handler, self))
        hev_task_sleep (1000);
}
