/*
 ============================================================================
 Name        : hev-task-io-us.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Task I/O operations
 ============================================================================
 */

#include <errno.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-circular-buffer.h>

#include "hev-task-io-us.h"

typedef struct _HevTaskIOSplicer HevTaskIOSplicer;

struct _HevTaskIOSplicer
{
    HevCircularBuffer *buf;
};

static int
task_io_splicer_init (HevTaskIOSplicer *self, size_t buf_size)
{
    self->buf = hev_circular_buffer_new (buf_size);
    if (!self->buf)
        return -1;

    return 0;
}

static void
task_io_splicer_fini (HevTaskIOSplicer *self)
{
    if (self->buf)
        hev_circular_buffer_unref (self->buf);
}

static int
task_io_splice (HevTaskIOSplicer *self, int fd_in, int fd_out)
{
    struct iovec iov[2];
    int res = 1, iovc;

    iovc = hev_circular_buffer_writing (self->buf, iov);
    if (iovc) {
        ssize_t s = readv (fd_in, iov, iovc);
        if (0 >= s) {
            if ((0 > s) && (EAGAIN == errno))
                res = 0;
            else
                res = -1;
        } else {
            hev_circular_buffer_write_finish (self->buf, s);
        }
    }

    iovc = hev_circular_buffer_reading (self->buf, iov);
    if (iovc) {
        ssize_t s = writev (fd_out, iov, iovc);
        if (0 >= s) {
            if ((0 > s) && (EAGAIN == errno))
                res = 0;
            else
                res = -1;
        } else {
            res = 1;
            hev_circular_buffer_read_finish (self->buf, s);
        }
    }

    return res;
}

void
hev_task_io_us_splice (int fd_a_i, int fd_a_o, int fd_b_i, int fd_b_o,
                       size_t buf_size, HevTaskIOYielder yielder,
                       void *yielder_data)
{
    HevTaskIOSplicer splicer_f;
    HevTaskIOSplicer splicer_b;
    int res_f = 1;
    int res_b = 1;

    if (task_io_splicer_init (&splicer_f, buf_size) < 0)
        return;
    if (task_io_splicer_init (&splicer_b, buf_size) < 0)
        goto exit;

    for (;;) {
        HevTaskYieldType type;

        if (res_f >= 0)
            res_f = task_io_splice (&splicer_f, fd_a_i, fd_b_o);
        if (res_b >= 0)
            res_b = task_io_splice (&splicer_b, fd_b_i, fd_a_o);

        if (fd_a_i == fd_a_o || fd_b_i == fd_b_o) {
            if (res_f < 0 || res_b < 0)
                break;
        } else {
            if (res_f < 0 && res_b < 0)
                break;
        }
        if (res_f > 0 || res_b > 0)
            type = HEV_TASK_YIELD;
        else
            type = HEV_TASK_WAITIO;

        if (yielder) {
            if (yielder (type, yielder_data))
                break;
        } else {
            hev_task_yield (type);
        }
    }

    task_io_splicer_fini (&splicer_b);
exit:
    task_io_splicer_fini (&splicer_f);
}
