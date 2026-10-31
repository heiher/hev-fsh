/*
 ============================================================================
 Name        : hev-bridge.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2026 hev
 Description : Bridge Win32 handles to a socket
 ============================================================================
 */

#ifdef _WIN32

#include <stdlib.h>

#include <hev-sys.h>
#include <windows.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>

#include "hev-bridge.h"

typedef struct _HevBridge HevBridge;

struct _HevBridge
{
    HANDLE in;
    HANDLE out;
    int flags;
    int fd;
    SOCKET sock;
    volatile LONG refs;
};

static void
hev_bridge_unref (HevBridge *self)
{
    if (InterlockedDecrement (&self->refs))
        return;

    hev_task_io_close (self->fd);
    if (self->flags & HEV_BRIDGE_OWN) {
        if (self->in)
            CloseHandle (self->in);
        if (self->out)
            CloseHandle (self->out);
    }
    free (self);
}

/* handle -> socket */
static DWORD WINAPI
hev_bridge_reader (LPVOID data)
{
    HevBridge *self = data;
    char buf[8192];
    int drop = 0;

    for (;;) {
        DWORD n = 0;
        int off = 0;

        if (!ReadFile (self->in, buf, sizeof (buf), &n, NULL) || !n)
            break;

        while (!drop && off < (int)n) {
            int s = send (self->sock, buf + off, n - off, 0);
            if (s > 0)
                off += s;
            else if (self->flags & HEV_BRIDGE_DRAIN)
                drop = 1;
            else
                goto exit;
        }
    }

exit:
    shutdown (self->sock, SD_SEND);
    hev_bridge_unref (self);
    return 0;
}

/* socket -> handle */
static DWORD WINAPI
hev_bridge_writer (LPVOID data)
{
    HevBridge *self = data;
    char buf[8192];
    char last = 0;

    for (;;) {
        int n = recv (self->sock, buf, sizeof (buf), 0);
        int off = 0;
        int i;

        if (n <= 0)
            break;

        if (self->flags & HEV_BRIDGE_LF2CR) {
            for (i = 0; i < n; i++) {
                char c = buf[i];

                if (c == '\n' && last != '\r')
                    buf[i] = '\r';
                last = c;
            }
        }

        while (off < n) {
            DWORD s = 0;

            if (!WriteFile (self->out, buf + off, n - off, &s, NULL) || !s)
                goto exit;
            off += s;
        }
    }

exit:
    /* Wake the reader up, and fail the peer's writes. */
    shutdown (self->sock, SD_BOTH);
    hev_bridge_unref (self);
    return 0;
}

static int
hev_bridge_spawn (HevBridge *self, LPTHREAD_START_ROUTINE entry)
{
    HANDLE thread;

    InterlockedIncrement (&self->refs);
    thread = CreateThread (NULL, 0, entry, self, 0, NULL);
    if (!thread) {
        InterlockedDecrement (&self->refs);
        return -1;
    }
    CloseHandle (thread);

    return 0;
}

int
hev_bridge_new (void *in, void *out, int flags)
{
    HevBridge *self;
    u_long blocking = 0;
    int sv[2];
    int fd;

    if (hev_task_io_socket_socketpair (PF_LOCAL, SOCK_STREAM, 0, sv) < 0)
        return -1;

    self = calloc (1, sizeof (HevBridge));
    if (!self)
        goto fail;

    self->in = in;
    self->out = out;
    self->flags = flags;
    self->fd = sv[1];
    self->sock = hev_task_io_socket_get_handle (sv[1]);
    self->refs = 1;

    /* The workers own sv[1] and use it in the blocking mode. */
    if (ioctlsocket (self->sock, FIONBIO, &blocking) != 0)
        goto fail_free;

    if (in && hev_bridge_spawn (self, hev_bridge_reader) < 0)
        goto fail_free;
    if (out && hev_bridge_spawn (self, hev_bridge_writer) < 0) {
        /* The reader may be running, let it release the last reference. */
        shutdown (self->sock, SD_BOTH);
        hev_bridge_unref (self);
        hev_task_io_close (sv[0]);
        return -1;
    }

    fd = sv[0];
    hev_bridge_unref (self);

    return fd;

fail_free:
    free (self);
fail:
    hev_task_io_close (sv[0]);
    hev_task_io_close (sv[1]);
    return -1;
}

#endif /* _WIN32 */
