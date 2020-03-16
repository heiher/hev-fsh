/*
 ============================================================================
 Name        : hev-fsh-config.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh Config
 ============================================================================
 */

#include <stdio.h>
#include <string.h>

#include "hev-fsh-config.h"
#include "hev-memory-allocator.h"

typedef struct _HevFshAddrListNode HevFshAddrListNode;

struct _HevFshConfig
{
    int mode;

    const char *server_address;
    unsigned int server_port;
    unsigned int timeout;

    const char *log;
    const char *user;
    const char *token;

    HevFshAddrListNode *addr_list;

    const char *local_address;
    unsigned int local_port;

    const char *remote_address;
    unsigned int remote_port;
};

struct _HevFshAddrListNode
{
    HevFshAddrListNode *next;

    int type;
    int action;

    unsigned char addr[16];
    unsigned int port;
};

HevFshConfig *
hev_fsh_config_new (void)
{
    HevFshConfig *self;

    self = hev_malloc0 (sizeof (HevFshConfig));
    if (!self) {
        fprintf (stderr, "Create fsh config failed!\n");
        return NULL;
    }

    self->timeout = 300;
    self->server_port = 6339;
    self->local_address = "127.0.0.1";

    return self;
}

void
hev_fsh_config_destroy (HevFshConfig *self)
{
    HevFshAddrListNode *iter = self->addr_list;

    while (iter) {
        HevFshAddrListNode *node = iter;

        iter = iter->next;
        hev_free (node);
    }

    hev_free (self);
}

int
hev_fsh_config_get_mode (HevFshConfig *self)
{
    return self->mode;
}

void
hev_fsh_config_set_mode (HevFshConfig *self, int val)
{
    self->mode = val;

    switch (val) {
    case HEV_FSH_CONFIG_MODE_SERVER:
        if (!self->server_address)
            self->server_address = "::";
        break;
    default:
        if (!self->server_address)
            self->server_address = "::1";
        break;
    }
}

const char *
hev_fsh_config_get_server_address (HevFshConfig *self)
{
    return self->server_address;
}

void
hev_fsh_config_set_server_address (HevFshConfig *self, const char *val)
{
    self->server_address = val;
}

unsigned int
hev_fsh_config_get_server_port (HevFshConfig *self)
{
    return self->server_port;
}

void
hev_fsh_config_set_server_port (HevFshConfig *self, unsigned int val)
{
    self->server_port = val;
}

const char *
hev_fsh_config_get_token (HevFshConfig *self)
{
    return self->token;
}

void
hev_fsh_config_set_token (HevFshConfig *self, const char *val)
{
    self->token = val;
}

const char *
hev_fsh_config_get_log (HevFshConfig *self)
{
    return self->log;
}

void
hev_fsh_config_set_log (HevFshConfig *self, const char *val)
{
    self->log = val;
}

unsigned int
hev_fsh_config_get_timeout (HevFshConfig *self)
{
    return self->timeout;
}

void
hev_fsh_config_set_timeout (HevFshConfig *self, unsigned int val)
{
    self->timeout = val;
}

const char *
hev_fsh_config_get_user (HevFshConfig *self)
{
    return self->user;
}

void
hev_fsh_config_set_user (HevFshConfig *self, const char *val)
{
    self->user = val;
}

int
hev_fsh_config_addr_list_contains (HevFshConfig *self, int type, void *addr,
                                   unsigned int port)
{
    HevFshAddrListNode *iter = self->addr_list;

    while (iter) {
        if (!iter->type)
            return iter->action;

        if (type == iter->type && port == iter->port) {
            switch (type) {
            case 4:
                if (0 == memcmp (addr, iter->addr, 4))
                    return iter->action;
                break;
            case 6:
                if (0 == memcmp (addr, iter->addr, 16))
                    return iter->action;
                break;
            }
        }

        iter = iter->next;
    }

    return 0;
}

void
hev_fsh_config_addr_list_add (HevFshConfig *self, int type, void *addr,
                              unsigned int port, int action)
{
    HevFshAddrListNode *node;

    node = hev_malloc0 (sizeof (HevFshAddrListNode));
    if (!node)
        return;

    node->type = type;
    node->port = port;
    node->action = action;
    switch (type) {
    case 4:
        memcpy (node->addr, addr, 4);
        break;
    case 6:
        memcpy (node->addr, addr, 16);
        break;
    }

    node->next = self->addr_list;
    self->addr_list = node;
}

const char *
hev_fsh_config_get_local_address (HevFshConfig *self)
{
    return self->local_address;
}

void
hev_fsh_config_set_local_address (HevFshConfig *self, const char *val)
{
    self->local_address = val;
}

unsigned int
hev_fsh_config_get_local_port (HevFshConfig *self)
{
    return self->local_port;
}

void
hev_fsh_config_set_local_port (HevFshConfig *self, unsigned int val)
{
    self->local_port = val;
}

const char *
hev_fsh_config_get_remote_address (HevFshConfig *self)
{
    return self->remote_address;
}

void
hev_fsh_config_set_remote_address (HevFshConfig *self, const char *val)
{
    self->remote_address = val;
}

unsigned int
hev_fsh_config_get_remote_port (HevFshConfig *self)
{
    return self->remote_port;
}

void
hev_fsh_config_set_remote_port (HevFshConfig *self, unsigned int val)
{
    self->remote_port = val;
}
