/*
 ============================================================================
 Name        : hev-fsh-config.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2020 everyone.
 Description : Fsh Config
 ============================================================================
 */

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <hev-task-call.h>

#include "hev-fsh-config.h"
#include "hev-memory-allocator.h"

typedef struct _HevTaskCallResolv HevTaskCallResolv;
typedef struct _HevFshAddrListNode HevFshAddrListNode;

struct _HevFshConfig
{
    int mode;
    int ip_type;

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

struct _HevTaskCallResolv
{
    HevTaskCall base;

    HevFshConfig *config;
    struct sockaddr *addr;
    socklen_t *len;
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
            self->server_address = "0.0.0.0";
        break;
    default:
        if (!self->server_address)
            self->server_address = "127.0.0.1";
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

int
hev_fsh_config_get_ip_type (HevFshConfig *self)
{
    return self->ip_type;
}

void
hev_fsh_config_set_ip_type (HevFshConfig *self, int val)
{
    self->ip_type = val;
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

static struct sockaddr *
parse_sockaddr (socklen_t *len, const char *address, int port)
{
    static struct sockaddr_storage addr;
    struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;

    __builtin_bzero (&addr, sizeof (addr));

    if (inet_pton (AF_INET, address, &addr4->sin_addr) == 1) {
        *len = sizeof (struct sockaddr_in);
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons (port);
        return (struct sockaddr *)addr4;
    }

    if (inet_pton (AF_INET6, address, &addr6->sin6_addr) == 1) {
        *len = sizeof (struct sockaddr_in6);
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons (port);
        return (struct sockaddr *)addr6;
    }

    return NULL;
}

static void
resolv_entry (HevTaskCall *call)
{
    HevTaskCallResolv *resolv = (HevTaskCallResolv *)call;
    const char *address = resolv->config->server_address;
    int port = resolv->config->server_port;
    static struct sockaddr_storage addr;
    struct hostent *h;

    switch (resolv->config->ip_type) {
    case 4:
        h = gethostbyname2 (address, AF_INET);
        break;
    case 6:
        h = gethostbyname2 (address, AF_INET6);
        break;
    default:
        h = gethostbyname (address);
        break;
    }

    if (!h) {
        hev_task_call_set_retval (call, NULL);
        return;
    }

    if (h->h_addrtype == AF_INET6) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;

        *resolv->len = sizeof (struct sockaddr_in6);
        __builtin_bzero (addr6, *resolv->len);
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons (port);
        memcpy (&addr6->sin6_addr, h->h_addr, sizeof (addr6->sin6_addr));
    } else {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;
        ;

        *resolv->len = sizeof (struct sockaddr_in);
        __builtin_bzero (addr4, *resolv->len);
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons (port);
        memcpy (&addr4->sin_addr, h->h_addr, sizeof (addr4->sin_addr));
    }

    hev_task_call_set_retval (call, &addr);
}

struct sockaddr *
hev_fsh_config_get_server_sockaddr (HevFshConfig *self, socklen_t *len)
{
    struct sockaddr *addr;

    addr = parse_sockaddr (len, self->server_address, self->server_port);
    if (!addr) {
        HevTaskCall *call;
        HevTaskCallResolv *resolv;

        call = hev_task_call_new (sizeof (HevTaskCallResolv), 16384);
        if (!call)
            return NULL;

        resolv = (HevTaskCallResolv *)call;
        resolv->config = self;
        resolv->addr = addr;
        resolv->len = len;

        addr = hev_task_call_jump (call, resolv_entry);
        hev_task_call_destroy (call);
    }

    return addr;
}

struct sockaddr *
hev_fsh_config_get_local_sockaddr (HevFshConfig *self, socklen_t *len)
{
    return parse_sockaddr (len, self->local_address, self->local_port);
}
