/*
 ============================================================================
 Name        : hev-random.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Random
 ============================================================================
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "hev-random.h"

void
hev_random_get_bytes (void *buf, size_t size)
{
    int fd;
    size_t i;
    unsigned char *cp = buf;
    static int init = 0;

    fd = open ("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        fd = open ("/dev/random", O_RDONLY | O_NONBLOCK | O_CLOEXEC);

    if (fd >= 0) {
        size_t n = size;
        int lose_counter = 0;

        while (n > 0) {
            ssize_t x = read (fd, cp, n);
            if (x <= 0) {
                if (lose_counter++ > 16)
                    break;
                continue;
            }
            n -= x;
            cp += x;
            lose_counter = 0;
        }

        close (fd);
        return;
    }

    if (!init) {
        struct timeval tv;

        gettimeofday (&tv, 0);
        srand (tv.tv_sec ^ tv.tv_usec);

        gettimeofday (&tv, 0);
        for (i = (tv.tv_sec ^ tv.tv_usec) & 0x1F; i > 0; i--)
            rand ();

        init = 1;
    }

    for (i = 0; i < size; i++)
        *cp++ = (rand () >> 7) & 0xFF;
}
