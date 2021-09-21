/*
 ============================================================================
 Name        : hev-fsh-client-port-accept.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client port accept
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

#include "hev-fsh-client-port-accept.h"

static void
hev_fsh_client_port_accept_task_entry (void *data)
{
    HevFshClientPortAccept *self = data;
    HevFshClientBase *base = data;
    HevFshMessagePortInfo mpinfo;
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int lfd;
    int rfd;
    int res;

    res = hev_fsh_client_accept_send_accept (&self->base);
    if (res < 0)
        goto quit;

    rfd = base->fd;
    /* recv message port info */
    res = hev_task_io_socket_recv (rfd, &mpinfo, sizeof (mpinfo), MSG_WAITALL,
                                   io_yielder, self);
    if (res <= 0)
        goto quit;

    res = hev_fsh_config_addr_list_contains (base->config, mpinfo.type,
                                             mpinfo.addr, mpinfo.port);
    if (res == 0)
        goto quit;

    switch (mpinfo.type) {
    case 4: {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;
        addr_len = sizeof (struct sockaddr_in);
        __builtin_bzero (addr4, addr_len);
        addr4->sin_family = AF_INET;
        addr4->sin_port = mpinfo.port;
        memcpy (&addr4->sin_addr, mpinfo.addr, sizeof (addr4->sin_addr));
        break;
    }
    case 6: {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;
        addr_len = sizeof (struct sockaddr_in6);
        __builtin_bzero (addr6, addr_len);
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = mpinfo.port;
        memcpy (&addr6->sin6_addr, mpinfo.addr, sizeof (addr6->sin6_addr));
        break;
    }
    default:
        goto quit;
    }

    lfd = hev_task_io_socket_socket (addr.ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (lfd < 0)
        goto quit;

    hev_task_add_fd (hev_task_self (), lfd, POLLIN | POLLOUT);

    res = hev_task_io_socket_connect (lfd, (struct sockaddr *)&addr, addr_len,
                                      io_yielder, self);
    if (res < 0)
        goto quit_close;

    if (hev_fsh_config_is_ugly_ktls (base->config))
        hev_task_io_us_splice (rfd, rfd, lfd, lfd, 8192, io_yielder, self);
    else
        hev_task_io_splice (rfd, rfd, lfd, lfd, 8192, io_yielder, self);

quit_close:
    close (lfd);
quit:
    hev_object_unref (HEV_OBJECT (self));
}

static void
hev_fsh_client_port_accept_run (HevFshIO *base)
{
    LOG_D ("%p fsh client port accept run", base);

    hev_task_run (base->task, hev_fsh_client_port_accept_task_entry, base);
}

HevFshClientBase *
hev_fsh_client_port_accept_new (HevFshConfig *config, HevFshToken token)
{
    HevFshClientPortAccept *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClientPortAccept));
    if (!self)
        return NULL;

    res = hev_fsh_client_port_accept_construct (self, config, token);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client port accept new", self);

    return HEV_FSH_CLIENT_BASE (self);
}

int
hev_fsh_client_port_accept_construct (HevFshClientPortAccept *self,
                                      HevFshConfig *config, HevFshToken token)
{
    int res;

    res = hev_fsh_client_accept_construct (&self->base, config, token);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client port accept construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_PORT_ACCEPT_TYPE;

    return 0;
}

static void
hev_fsh_client_port_accept_destruct (HevObject *base)
{
    HevFshClientPortAccept *self = HEV_FSH_CLIENT_PORT_ACCEPT (base);

    LOG_D ("%p fsh client port accept destruct", self);

    HEV_FSH_CLIENT_ACCEPT_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_client_port_accept_class (void)
{
    static HevFshClientPortAcceptClass klass;
    HevFshClientPortAcceptClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshIOClass *ikptr;
        void *ptr;

        ptr = HEV_FSH_CLIENT_ACCEPT_TYPE;
        memcpy (kptr, ptr, sizeof (HevFshClientAcceptClass));

        okptr->name = "HevFshClientPortAccept";
        okptr->finalizer = hev_fsh_client_port_accept_destruct;

        ikptr = HEV_FSH_IO_CLASS (kptr);
        ikptr->run = hev_fsh_client_port_accept_run;
    }

    return okptr;
}
