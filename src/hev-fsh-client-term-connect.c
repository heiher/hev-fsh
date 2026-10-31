/*
 ============================================================================
 Name        : hev-fsh-client-term-connect.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client term connect
 ============================================================================
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <hev-sys.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <sys/ioctl.h>
#endif

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-memory-allocator.h>

#include "hev-bridge.h"
#include "hev-logger.h"
#include "hev-task-io-us.h"
#include "hev-fsh-protocol.h"

#include "hev-fsh-client-term-connect.h"

#ifdef _WIN32

#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif

static HANDLE con_in;
static HANDLE con_out;
static DWORD con_in_mode;
static DWORD con_out_mode;
static UINT con_in_cp;
static UINT con_out_cp;

static void
console_restore (void)
{
    SetConsoleMode (con_in, con_in_mode);
    SetConsoleMode (con_out, con_out_mode);
    SetConsoleCP (con_in_cp);
    SetConsoleOutputCP (con_out_cp);
}

/* Pass through raw bytes and escape sequences, like the raw tty mode. */
static int
console_raw (void)
{
    DWORD mode;

    con_in = GetStdHandle (STD_INPUT_HANDLE);
    con_out = GetStdHandle (STD_OUTPUT_HANDLE);

    if (!GetConsoleMode (con_in, &con_in_mode))
        return -1;
    if (!GetConsoleMode (con_out, &con_out_mode))
        return -1;
    con_in_cp = GetConsoleCP ();
    con_out_cp = GetConsoleOutputCP ();

    mode = ENABLE_VIRTUAL_TERMINAL_INPUT;
    if (!SetConsoleMode (con_in, mode))
        return -1;

    mode = con_out_mode | ENABLE_PROCESSED_OUTPUT |
           ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    if (!SetConsoleMode (con_out, mode)) {
        SetConsoleMode (con_in, con_in_mode);
        return -1;
    }

    SetConsoleCP (CP_UTF8);
    SetConsoleOutputCP (CP_UTF8);
    atexit (console_restore);

    return 0;
}

static void
hev_fsh_client_term_connect_task_entry (void *data)
{
    HevFshClientTermConnect *self = data;
    HevFshClientBase *base = data;
    HevFshMessageTermInfo mtinfo;
    CONSOLE_SCREEN_BUFFER_INFO info;
    int res;
    int fd;
    int cfd;

    res = hev_fsh_client_connect_send_connect (&self->base);
    if (res < 0)
        goto exit;

    if (!GetConsoleScreenBufferInfo (GetStdHandle (STD_OUTPUT_HANDLE), &info))
        goto exit;

    fd = base->fd;
    mtinfo.rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    mtinfo.columns = info.srWindow.Right - info.srWindow.Left + 1;

    /* send message term info */
    res = hev_task_io_socket_send (fd, &mtinfo, sizeof (mtinfo), MSG_WAITALL,
                                   io_yielder, self);
    if (res <= 0)
        goto exit;

    if (console_raw () < 0)
        goto exit;

    cfd = hev_bridge_new (con_in, con_out, 0);
    if (cfd < 0) {
        console_restore ();
        goto exit;
    }

    hev_task_add_fd (hev_task_self (), cfd, POLLIN | POLLOUT);
    hev_task_io_splice (fd, fd, cfd, cfd, 8192, io_yielder, self);

    hev_task_io_close (cfd);
    console_restore ();

exit:
    hev_object_unref (HEV_OBJECT (self));
}

#else

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
    int fd;

    res = hev_fsh_client_connect_send_connect (&self->base);
    if (res < 0)
        goto exit;

    res = ioctl (0, TIOCGWINSZ, &win_size);
    if (res < 0)
        goto exit;

    fd = base->fd;
    mtinfo.rows = win_size.ws_row;
    mtinfo.columns = win_size.ws_col;

    /* send message term info */
    res = hev_task_io_socket_send (fd, &mtinfo, sizeof (mtinfo), MSG_WAITALL,
                                   io_yielder, self);
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

    if (hev_fsh_config_is_ugly_ktls (base->config))
        hev_task_io_us_splice (fd, fd, 0, 1, 8192, io_yielder, self);
    else
        hev_task_io_splice (fd, fd, 0, 1, 8192, io_yielder, self);

    tcsetattr (0, TCSADRAIN, &term);

exit:
    hev_object_unref (HEV_OBJECT (self));
}

#endif

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

    HEV_FSH_CLIENT_CONNECT_TYPE->destruct (base);
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
        okptr->destruct = hev_fsh_client_term_connect_destruct;

        ikptr = HEV_FSH_IO_CLASS (kptr);
        ikptr->run = hev_fsh_client_term_connect_run;
    }

    return okptr;
}
