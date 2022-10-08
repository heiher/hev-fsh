/*
 ============================================================================
 Name        : hev-fsh-config.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh Config
 ============================================================================
 */

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>

#include <hev-task-dns.h>
#include <hev-task-call.h>
#include <hev-strverscmp.h>

#include "hev-fsh-config.h"
#include "hev-memory-allocator.h"

typedef struct _HevTaskCallResolv HevTaskCallResolv;
typedef struct _HevFshAddrListNode HevFshAddrListNode;

struct _HevFshConfig
{
    int mode;
    int crypto;
    int ip_type;
    int log_level;
    int ugly_ktls;

    const char *server_address;
    const char *server_port;
    unsigned int timeout;

    const char *user;
    const char *token;
    const char *log_path;

    HevFshAddrListNode *addr_list;

    const char *local_address;
    unsigned int local_port;

    const char *remote_address;
    unsigned int remote_port;

    HevFshConfigKey key;
};

struct _HevTaskCallResolv
{
    HevTaskCall base;

    HevFshConfig *config;
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

    self->timeout = 120;
    self->server_port = "6339";
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

const char *
hev_fsh_config_get_server_port (HevFshConfig *self)
{
    return self->server_port;
}

void
hev_fsh_config_set_server_port (HevFshConfig *self, const char *val)
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
hev_fsh_config_get_log_path (HevFshConfig *self)
{
    return self->log_path;
}

void
hev_fsh_config_set_log_path (HevFshConfig *self, const char *val)
{
    self->log_path = val;
}

int
hev_fsh_config_get_log_level (HevFshConfig *self)
{
    return self->log_level;
}

void
hev_fsh_config_set_log_level (HevFshConfig *self, int val)
{
    self->log_level = val;
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

HevFshConfigKey *
hev_fsh_config_get_key (HevFshConfig *self)
{
    if (self->crypto)
        return &self->key;

    return NULL;
}

static int
is_ugly_ktls (void)
{
#ifdef __linux__
    struct utsname u;
    int res;

    res = uname (&u);
    if (res < 0)
        return 1;

    /* The kernel TLS with splice syscall is not working before v5.14. */
    res = hev_strverscmp (u.release, "5.14");
    if (res < 0)
        return 1;
#endif

    return 0;
}

void
hev_fsh_config_set_key (HevFshConfig *self, HevFshConfigKey *val)
{
    if (val) {
        self->crypto = 1;
        self->ugly_ktls = is_ugly_ktls ();
        memcpy (&self->key, val, sizeof (self->key));
    } else {
        self->crypto = 0;
        self->ugly_ktls = 0;
    }
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
    const char *port = resolv->config->server_port;
    static struct sockaddr_storage addr;
    struct addrinfo *res = NULL;
    struct addrinfo hints;
    int s;

    __builtin_bzero (&hints, sizeof (hints));
    switch (resolv->config->ip_type) {
    case 4:
        hints.ai_family = AF_INET;
        break;
    case 6:
        hints.ai_family = AF_INET6;
        break;
    default:
        hints.ai_family = AF_UNSPEC;
        break;
    }
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    s = hev_task_dns_getaddrinfo (address, port, &hints, &res);
    if ((s != 0) || !res) {
        hev_task_call_set_retval (call, NULL);
        return;
    }

    *resolv->len = res->ai_addrlen;
    memcpy (&addr, res->ai_addr, res->ai_addrlen);

    if (res->ai_family == AF_INET6) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;
        struct in6_addr *pa = &addr6->sin6_addr;
        unsigned short *pp = &addr6->sin6_port;
        if ((pa->s6_addr[0] == 0x20) && (pa->s6_addr[1] == 0x01) &&
            (pa->s6_addr[2] == 0x00) && (pa->s6_addr[3] == 0x00)) {
            memcpy (pp, &pa->s6_addr[10], 2);
            pa->s6_addr[10] = 0xff;
            pa->s6_addr[11] = 0xff;
            memset (pa, 0, 10);
        }
    }

    hev_task_call_set_retval (call, &addr);
    freeaddrinfo (res);
}

struct sockaddr *
hev_fsh_config_get_server_sockaddr (HevFshConfig *self, socklen_t *len)
{
    struct sockaddr *addr;

    addr = parse_sockaddr (len, self->server_address, atoi (self->server_port));
    if (!addr) {
        HevTaskCall *call;
        HevTaskCallResolv *resolv;

        call = hev_task_call_new (sizeof (HevTaskCallResolv), 16384);
        if (!call)
            return NULL;

        resolv = (HevTaskCallResolv *)call;
        resolv->config = self;
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

int
hev_fsh_config_is_ugly_ktls (HevFshConfig *self)
{
    return self->ugly_ktls;
}
