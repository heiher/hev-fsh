/*
 ============================================================================
 Name        : hev-fsh-client-sock-connect.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh client sock connect
 ============================================================================
 */

#include <string.h>
#include <unistd.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-task-io-us.h"
#include "hev-fsh-protocol.h"

#include "hev-fsh-client-sock-connect.h"

static void
hev_fsh_client_sock_connect_task_entry (void *data)
{
    HevFshClientSockConnect *self = data;
    HevFshClientBase *base = data;
    int sfd;
    int bfd;
    int res;

    res = hev_fsh_client_connect_send_connect (&self->base);
    if (res < 0)
        goto exit;

    sfd = self->fd;
    bfd = base->fd;
    hev_task_add_fd (hev_task_self (), sfd, POLLIN | POLLOUT);

    if (hev_fsh_config_is_ugly_ktls (base->config))
        hev_task_io_us_splice (bfd, bfd, sfd, sfd, 8192, io_yielder, self);
    else
        hev_task_io_splice (bfd, bfd, sfd, sfd, 8192, io_yielder, self);

exit:
    hev_object_unref (HEV_OBJECT (self));
}

static void
hev_fsh_client_sock_connect_run (HevFshIO *base)
{
    LOG_D ("%p fsh client sock connect run", base);

    hev_task_run (base->task, hev_fsh_client_sock_connect_task_entry, base);
}

HevFshClientBase *
hev_fsh_client_sock_connect_new (HevFshConfig *config, int fd)
{
    HevFshClientSockConnect *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClientSockConnect));
    if (!self)
        return NULL;

    res = hev_fsh_client_sock_connect_construct (self, config, fd);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client sock connect new", self);

    return HEV_FSH_CLIENT_BASE (self);
}

int
hev_fsh_client_sock_connect_construct (HevFshClientSockConnect *self,
                                       HevFshConfig *config, int fd)
{
    int res;

    res = hev_fsh_client_connect_construct (&self->base, config);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client sock connect construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_SOCK_CONNECT_TYPE;

    self->fd = fd;

    return 0;
}

static void
hev_fsh_client_sock_connect_destruct (HevObject *base)
{
    HevFshClientSockConnect *self = HEV_FSH_CLIENT_SOCK_CONNECT (base);

    LOG_D ("%p fsh client sock connect destruct", self);

    if (self->fd >= 0)
        close (self->fd);

    HEV_FSH_CLIENT_CONNECT_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_client_sock_connect_class (void)
{
    static HevFshClientSockConnectClass klass;
    HevFshClientSockConnectClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshIOClass *ikptr;
        void *ptr;

        ptr = HEV_FSH_CLIENT_CONNECT_TYPE;
        memcpy (kptr, ptr, sizeof (HevFshClientConnectClass));

        okptr->name = "HevFshClientSockConnect";
        okptr->finalizer = hev_fsh_client_sock_connect_destruct;

        ikptr = HEV_FSH_IO_CLASS (kptr);
        ikptr->run = hev_fsh_client_sock_connect_run;
    }

    return okptr;
}
