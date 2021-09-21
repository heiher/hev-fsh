/*
 ============================================================================
 Name        : hev-socks5-server-us.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2021 hev
 Description : Socks5 Server Userspace
 ============================================================================
 */

#include <string.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-task-io-us.h"
#include "hev-socks5-misc.h"

#include "hev-socks5-server-us.h"

#define task_io_yielder hev_socks5_task_io_yielder

static int
hev_socks5_server_us_tcp_splicer (HevSocks5TCP *tcp, int fd)
{
    HevTask *task = hev_task_self ();
    int cfd;
    int res;

    LOG_D ("%p socks5 server us tcp splicer", tcp);

    cfd = HEV_SOCKS5 (tcp)->fd;
    if (cfd < 0)
        return -1;

    res = hev_task_add_fd (task, fd, POLLIN | POLLOUT);
    if (res < 0)
        hev_task_mod_fd (task, fd, POLLIN | POLLOUT);

    hev_task_io_us_splice (cfd, cfd, fd, fd, 8192, task_io_yielder, tcp);

    return 0;
}

HevSocks5Server *
hev_socks5_server_us_new (int fd)
{
    HevSocks5ServerUS *self;
    int res;

    self = hev_malloc0 (sizeof (HevSocks5ServerUS));
    if (!self)
        return NULL;

    res = hev_socks5_server_us_construct (self, fd);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p socks5 server us new", self);

    return HEV_SOCKS5_SERVER (self);
}

int
hev_socks5_server_us_construct (HevSocks5ServerUS *self, int fd)
{
    int res;

    res = hev_socks5_server_construct (&self->base, fd);
    if (res < 0)
        return res;

    LOG_D ("%p socks5 server us construct", self);

    HEV_OBJECT (self)->klass = HEV_SOCKS5_SERVER_US_TYPE;

    return 0;
}

static void
hev_socks5_server_us_destruct (HevObject *base)
{
    HevSocks5ServerUS *self = HEV_SOCKS5_SERVER_US (base);

    LOG_D ("%p socks5 server us destruct", self);

    HEV_SOCKS5_SERVER_TYPE->finalizer (base);
}

HevObjectClass *
hev_socks5_server_us_class (void)
{
    static HevSocks5ServerUSClass klass;
    HevSocks5ServerUSClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevSocks5TCPIface *tiptr;
        memcpy (kptr, HEV_SOCKS5_SERVER_TYPE, sizeof (HevSocks5ServerClass));

        okptr->name = "HevSocks5ServerUS";
        okptr->finalizer = hev_socks5_server_us_destruct;

        tiptr = &kptr->base.tcp;
        tiptr->splicer = hev_socks5_server_us_tcp_splicer;
    }

    return okptr;
}
