/*
 ============================================================================
 Name        : hev-fsh-client-base.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client base
 ============================================================================
 */

#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#ifdef __linux__
#include <linux-tls.h>
#include <netinet/tcp.h>
#endif

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-random.h"

#include "hev-fsh-client-base.h"

#ifndef SOL_TLS
#define SOL_TLS 282
#endif

#ifndef TCP_ULP
#define TCP_ULP 31
#endif

static int
hev_fsh_client_base_socket (HevFshClientBase *self, int family)
{
    int flags;
    int res;
    int fd;

    fd = hev_task_io_socket_socket (family, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
        return -1;

    flags = fcntl (fd, F_GETFD);
    if (flags < 0) {
        LOG_E ("%p fsh client base fcntl", self);
        close (fd);
        return -1;
    }

    flags |= FD_CLOEXEC;
    res = fcntl (fd, F_SETFD, flags);
    if (res < 0) {
        LOG_E ("%p fsh client base cloexec", self);
        close (fd);
        return -1;
    }

    return fd;
}

int
hev_fsh_client_base_listen (HevFshClientBase *self)
{
    struct sockaddr *addr;
    socklen_t addr_len;
    int one = 1;
    int res;
    int fd;

    addr = hev_fsh_config_get_local_sockaddr (self->config, &addr_len);
    if (!addr) {
        LOG_E ("%p fsh client base addr", self);
        return -1;
    }

    fd = hev_fsh_client_base_socket (self, addr->sa_family);
    if (fd < 0)
        return -1;

    res = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));
    if (res < 0) {
        LOG_E ("%p fsh client base reuse", self);
        return -1;
    }

    res = bind (fd, addr, addr_len);
    if (res < 0) {
        LOG_E ("%p fsh client base bind", self);
        close (fd);
        return -1;
    }

    res = listen (fd, 10);
    if (res < 0) {
        LOG_E ("%p fsh client base listen", self);
        close (fd);
        return -1;
    }

    hev_task_add_fd (hev_task_self (), fd, POLLIN);
    self->fd = fd;

    return 0;
}

int
hev_fsh_client_base_connect (HevFshClientBase *self)
{
    struct sockaddr *addr;
    socklen_t addr_len;
    const char *cc;
    int res;
    int fd;

    addr = hev_fsh_config_get_server_sockaddr (self->config, &addr_len);
    if (!addr) {
        LOG_E ("%p fsh client base addr", self);
        return -1;
    }

    fd = hev_fsh_client_base_socket (self, addr->sa_family);
    if (fd < 0)
        return -1;

    cc = hev_fsh_config_get_tcp_cc (self->config);
    if (cc) {
#ifdef __linux__
        res = setsockopt (fd, IPPROTO_TCP, TCP_CONGESTION, cc, strlen (cc));
        if (res < 0)
            LOG_W ("%p fsh client base tcp congestion", self);
#endif
    }

    hev_task_add_fd (hev_task_self (), fd, POLLIN | POLLOUT);

    res = hev_task_io_socket_connect (fd, addr, addr_len, io_yielder, self);
    if (res < 0) {
        LOG_E ("%p fsh client base connect", self);
        close (fd);
        return -1;
    }

    self->fd = fd;

    return 0;
}

int
hev_fsh_client_base_encrypt (HevFshClientBase *self)
{
#ifdef __linux__
    struct tls12_crypto_info_aes_gcm_128 ci;
    HevFshConfigKey *key;
    int res;

    LOG_D ("%p fsh client base encrypt", self);

    key = hev_fsh_config_get_key (self->config);
    if (!key)
        return 0;

    ci.info.version = TLS_1_2_VERSION;
    ci.info.cipher_type = TLS_CIPHER_AES_GCM_128;
    hev_random_get_bytes (ci.iv, TLS_CIPHER_AES_GCM_128_IV_SIZE);
    memcpy (ci.key, key->key, TLS_CIPHER_AES_GCM_128_KEY_SIZE);
    memcpy (ci.salt, key->salt, TLS_CIPHER_AES_GCM_128_SALT_SIZE);
    memset (ci.rec_seq, 0, TLS_CIPHER_AES_GCM_128_REC_SEQ_SIZE);

    res = hev_task_io_socket_send (self->fd, ci.iv, sizeof (ci.iv), MSG_WAITALL,
                                   io_yielder, self);
    if (res <= 0)
        return -1;

    res = setsockopt (self->fd, SOL_TCP, TCP_ULP, "tls", sizeof ("tls"));
    if (res < 0) {
        LOG_E ("%p fsh client base tls (modprobe tls)", self);
        return -1;
    }

    res = setsockopt (self->fd, SOL_TLS, TLS_TX, &ci, sizeof (ci));
    if (res < 0)
        return -1;

    res = hev_task_io_socket_recv (self->fd, ci.iv, sizeof (ci.iv), MSG_WAITALL,
                                   io_yielder, self);
    if (res != sizeof (ci.iv))
        return -1;

    res = setsockopt (self->fd, SOL_TLS, TLS_RX, &ci, sizeof (ci));
    if (res < 0)
        return -1;

#endif
    return 0;
}

int
hev_fsh_client_base_construct (HevFshClientBase *self, HevFshConfig *config)
{
    int res;
    unsigned int timeout;

    timeout = hev_fsh_config_get_timeout (config);
    res = hev_fsh_io_construct (&self->base, timeout);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client base construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_BASE_TYPE;

    self->fd = -1;
    self->config = config;
    signal (SIGCHLD, SIG_IGN);

    return 0;
}

static void
hev_fsh_client_base_destruct (HevObject *base)
{
    HevFshClientBase *self = HEV_FSH_CLIENT_BASE (base);

    LOG_D ("%p fsh client base destruct", self);

    if (self->fd >= 0)
        close (self->fd);

    HEV_FSH_IO_TYPE->destruct (base);
}

HevObjectClass *
hev_fsh_client_base_class (void)
{
    static HevFshClientBaseClass klass;
    HevFshClientBaseClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        memcpy (kptr, HEV_FSH_IO_TYPE, sizeof (HevFshIOClass));

        okptr->name = "HevFshClientBase";
        okptr->destruct = hev_fsh_client_base_destruct;
    }

    return okptr;
}
