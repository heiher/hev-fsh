/*
 ============================================================================
 Name        : hev-fsh-client-term-connect.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client term connect
 ============================================================================
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-fsh-protocol.h"

#include "hev-fsh-client-term-connect.h"

static void
hev_fsh_client_term_connect_task_entry (void *data)
{
    HevFshClientTermConnect *self = data;
    HevFshClientBase *base = data;
    HevFshMessageTermInfo mtinfo;
    struct winsize win_size;
    struct termios term_rsh;
    struct termios term;
    int res;

    res = hev_fsh_client_connect_send_connect (&self->base);
    if (res < 0)
        goto exit;

    res = ioctl (0, TIOCGWINSZ, &win_size);
    if (res < 0)
        goto exit;

    mtinfo.rows = win_size.ws_row;
    mtinfo.columns = win_size.ws_col;

    /* send message term info */
    res = hev_task_io_socket_send (base->fd, &mtinfo, sizeof (mtinfo),
                                   MSG_WAITALL, io_yielder, self);
    if (res <= 0)
        goto exit;

    res = fcntl (0, F_SETFL, O_NONBLOCK);
    if (res < 0)
        goto exit;
    res = fcntl (1, F_SETFL, O_NONBLOCK);
    if (res < 0)
        goto exit;

    hev_task_add_fd (hev_task_self (), 0, POLLIN);
    hev_task_add_fd (hev_task_self (), 1, POLLOUT);

    res = tcgetattr (0, &term);
    if (res < 0)
        goto exit;

    memcpy (&term_rsh, &term, sizeof (term));
    term_rsh.c_oflag &= ~(OPOST);
    term_rsh.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK);
    res = tcsetattr (0, TCSADRAIN, &term_rsh);
    if (res < 0)
        goto exit;

    hev_task_io_splice (base->fd, base->fd, 0, 1, 8192, io_yielder, self);

    tcsetattr (0, TCSADRAIN, &term);

exit:
    hev_object_unref (HEV_OBJECT (self));
}

static void
hev_fsh_client_term_connect_run (HevFshIO *base)
{
    LOG_D ("%p fsh client term connect run", base);

    hev_task_run (base->task, hev_fsh_client_term_connect_task_entry, base);
}

HevFshClientBase *
hev_fsh_client_term_connect_new (HevFshConfig *config)
{
    HevFshClientTermConnect *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshClientTermConnect));
    if (!self)
        return NULL;

    res = hev_fsh_client_term_connect_construct (self, config);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh client term connect new", self);

    return HEV_FSH_CLIENT_BASE (self);
}

int
hev_fsh_client_term_connect_construct (HevFshClientTermConnect *self,
                                       HevFshConfig *config)
{
    int res;

    res = hev_fsh_client_connect_construct (&self->base, config);
    if (res < 0)
        return res;

    LOG_D ("%p fsh client term connect construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_CLIENT_TERM_CONNECT_TYPE;

    return 0;
}

static void
hev_fsh_client_term_connect_destruct (HevObject *base)
{
    HevFshClientTermConnect *self = HEV_FSH_CLIENT_TERM_CONNECT (base);

    LOG_D ("%p fsh client term connect destruct", self);

    HEV_FSH_CLIENT_CONNECT_TYPE->finalizer (base);
}

HevObjectClass *
hev_fsh_client_term_connect_class (void)
{
    static HevFshClientTermConnectClass klass;
    HevFshClientTermConnectClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        HevFshIOClass *ikptr;
        void *ptr;

        ptr = HEV_FSH_CLIENT_CONNECT_TYPE;
        memcpy (kptr, ptr, sizeof (HevFshClientConnectClass));

        okptr->name = "HevFshClientTermConnect";
        okptr->finalizer = hev_fsh_client_term_connect_destruct;

        ikptr = HEV_FSH_IO_CLASS (kptr);
        ikptr->run = hev_fsh_client_term_connect_run;
    }

    return okptr;
}
