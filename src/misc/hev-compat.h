/*
 ============================================================================
 Name        : hev-compat.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2026 hev
 Description : Compatibility
 ============================================================================
 */

#ifndef __HEV_COMPAT_H__
#define __HEV_COMPAT_H__

#if defined(_WIN32) && !defined(__MSYS__) && !defined(__CYGWIN__)
#define HEV_WIN_NATIVE 1
#endif

#ifdef HEV_WIN_NATIVE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hev-sys.h>
#include <windows.h>

#define strcasecmp _stricmp

static char *optarg;
static int optind = 1;
static int optopt;

/* Move the non-options behind the options, like the GNU getopt does. */
static inline void
getopt_permute (int argc, char *argv[], const char *optstring)
{
    char **tmp;
    int i, j = 1, k;

    tmp = malloc (sizeof (char *) * argc);
    if (!tmp)
        return;

    /* options (and their arguments) first, in order */
    for (i = 1; i < argc; i++) {
        const char *p;

        if (argv[i][0] != '-' || argv[i][1] == '\0')
            continue;
        if (argv[i][1] == '-' && argv[i][2] == '\0')
            break;

        tmp[j++] = argv[i];
        p = strchr (optstring, argv[i][1]);
        if (p && p[1] == ':' && !argv[i][2] && i + 1 < argc) {
            tmp[j++] = argv[i + 1];
            i++;
        }
    }
    k = j;

    /* then the non-options, and everything after "--" */
    for (i = 1; i < argc; i++) {
        int taken = 0;
        int n;

        for (n = 1; n < k && !taken; n++)
            taken = tmp[n] == argv[i];
        if (!taken)
            tmp[j++] = argv[i];
    }

    for (i = 1; i < argc; i++)
        argv[i] = tmp[i];
    free (tmp);
}

/* Minimal getopt, short options only. */
static inline int
getopt (int argc, char *const argv[], const char *optstring)
{
    static int pos = 1;
    static int init;
    const char *p;
    char *arg;

    if (!init) {
        getopt_permute (argc, (char **)argv, optstring);
        init = 1;
    }

    if (optind >= argc)
        return -1;

    arg = argv[optind];
    if (arg[0] != '-' || arg[1] == '\0')
        return -1;

    if (arg[1] == '-' && arg[2] == '\0') {
        optind++;
        return -1;
    }

    optopt = arg[pos];
    p = strchr (optstring, optopt);
    if (optopt == ':' || !p) {
        if (!arg[++pos]) {
            optind++;
            pos = 1;
        }
        return '?';
    }

    if (p[1] == ':') {
        if (arg[pos + 1]) {
            optarg = &arg[pos + 1];
        } else if (optind + 1 < argc) {
            optarg = argv[++optind];
        } else {
            optind++;
            pos = 1;
            return '?';
        }
        optind++;
        pos = 1;
    } else {
        if (!arg[++pos]) {
            optind++;
            pos = 1;
        }
    }

    return optopt;
}

static inline ssize_t
getline (char **lineptr, size_t *n, FILE *stream)
{
    size_t len = 0;
    int c;

    if (!lineptr || !n || !stream)
        return -1;

    if (!*lineptr || *n < 2) {
        char *buf = realloc (*lineptr, 128);
        if (!buf)
            return -1;
        *lineptr = buf;
        *n = 128;
    }

    while ((c = fgetc (stream)) != EOF) {
        if (len + 2 > *n) {
            char *buf = realloc (*lineptr, *n * 2);
            if (!buf)
                return -1;
            *lineptr = buf;
            *n *= 2;
        }
        (*lineptr)[len++] = c;
        if (c == '\n')
            break;
    }

    if (len == 0)
        return -1;

    (*lineptr)[len] = '\0';
    return len;
}

#else

#include <unistd.h>

#endif

#endif /* __HEV_COMPAT_H__ */
