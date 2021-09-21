/*
 ============================================================================
 Name        : hev-fsh-client-port-connect.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client port connect
 ============================================================================
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-task-io-us.h"
#include "hev-fsh-protocol.h"

#include "hev-fsh-client-port-connect.h"

static void
hev_fsh_client_port_connect_task_entry (void *data)
{
    HevFshClientPortConnect *self = data;
    HevFshClientBase *base = data;
    HevFshMessagePortInfo mpinfo;
    HevTask *task = hev_task_self ();
    const char *addr;
    int port;
    int ifd;
    int ofd;
    int bfd;
    int res;

    res = hev_fsh_client_connect_send_connect (&self->base);
    if (res < 0)
        goto exit;

    addr = hev_fsh_config_get_remote_address (base->config);
    port = hev_fsh_config_get_remote_port (base->config);

    __builtin_bzero (mpinfo.addr, sizeof (mpinfo.addr));
    mpinfo.port = htons (port);
    bfd = base->fd;

    if (inet_pton (AF_INET, addr, mpinfo.addr) == 1) {
        mpinfo.type = 4;
    } else {
        mpinfo.type = 6;
        inet_pton (AF_INET6, addr, mpinfo.addr);
    }

    /* send message port info */
    res = hev_task_io_socket_send (bfd, &mpinfo, sizeof (mpinfo), MSG_WAITALL,
                                   io_yielder, self);
    if (res <= 0)
        goto exit;

    if (self->fd < 0) {
        ifd = 0;
        ofd = 1;
    } else {
        ifd = self->fd;
        ofd = self->fd;
    }

    res = fcntl (ifd, F_SETFL, O_NONBLOCK);
    if (res < 0)
        goto exit;

    if (ifd != ofd) {
        res = fcntl (ofd, F_SETFL, O_NONBLOCK);
        if (res < 0)
            goto exit;

        hev_task_add_fd (task, ifd, POLLIN);
        hev_task_add_fd (task, ofd, POLLOUT);
    } else {
        hev_task_add_fd (task, ifd, POLLIN | POLLOUT);
    }

    if (hev_fsh_config_is_ugly_ktls (base->config))
        hev_task_io_us_splice (bfd, bfd, ifd, ofd, 8192, io_yielder, self);
    else
        hev_task_io_splice (bfd, bfd, ifd, ofd, 8192, io_yielder, self);

exit:
    hev_object_unref (HEV_OBJECT (self));
}

static void
hev_fsh_client_port_connect_run (HevFshIO *base)
{
    LOG_D ("%p fsh client port connect run", base);

    hev_task_run (base->task, hev_fsh_client_port_connect_task_entry, base);
}

HevFshClientBase *
hev_fsh_client_port_connect_new (HevFshConfig *config, int fd)
{
    HevFshClientPortConnect *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClientPortConnect));
    if (!self)
        return NULL;

    res = hev_fsh_client_port_connect_construct (self, config, fd);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client port connect new", self);

    return HEV_FSH_CLIENT_BASE (self);
}

int
hev_fsh_client_port_connect_construct (HevFshClientPortConnect *self,
                                       HevFshConfig *config, int fd)
{
    int res;

    res = hev_fsh_client_connect_construct (&self->base, config);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client port connect construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_PORT_CONNECT_TYPE;

    self->fd = fd;

    return 0;
}

static void
hev_fsh_client_port_connect_destruct (HevObject *base)
{
    HevFshClientPortConnect *self = HEV_FSH_CLIENT_PORT_CONNECT (base);

    LOG_D ("%p fsh client port connect destruct", self);

    if (self->fd >= 0)
        close (self->fd);

    HEV_FSH_CLIENT_CONNECT_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_client_port_connect_class (void)
{
    static HevFshClientPortConnectClass klass;
    HevFshClientPortConnectClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshIOClass *ikptr;
        void *ptr;

        ptr = HEV_FSH_CLIENT_CONNECT_TYPE;
        memcpy (kptr, ptr, sizeof (HevFshClientConnectClass));

        okptr->name = "HevFshClientPortConnect";
        okptr->finalizer = hev_fsh_client_port_connect_destruct;

        ikptr = HEV_FSH_IO_CLASS (kptr);
        ikptr->run = hev_fsh_client_port_connect_run;
    }

    return okptr;
}
