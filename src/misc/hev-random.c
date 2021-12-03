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
    static int init = 0;
    static int fd;
    unsigned char *cp = buf;
    size_t i;

    if (!init) {
        struct timeval tv;

        fd = open ("/dev/urandom", O_RDONLY | O_CLOEXEC);
        if (fd < 0)
            fd = open ("/dev/random", O_RDONLY | O_NONBLOCK | O_CLOEXEC);

        gettimeofday (&tv, 0);
        srand (tv.tv_sec ^ tv.tv_usec);

        gettimeofday (&tv, 0);
        for (i = (tv.tv_sec ^ tv.tv_usec) & 0x1F; i > 0; i--)
            rand ();

        init = 1;
    }

    if (fd >= 0) {
        int lose_counter = 0;

        while (size > 0) {
            ssize_t x = read (fd, cp, size);
            if (x <= 0) {
                if (lose_counter++ > 16)
                    break;
                continue;
            }
            size -= x;
            cp += x;
            lose_counter = 0;
        }

        if (size == 0)
            return;
    }

    for (i = 0; i < size; i++)
        *cp++ = (rand () >> 7) & 0xFF;
}
