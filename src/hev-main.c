/*
 ============================================================================
 Name        : hev-main.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2021 xyz
 Description : Main
 ============================================================================
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <hev-task-system.h>

#include "hev-logger.h"
#include "hev-fsh-config.h"
#include "hev-fsh-server.h"
#include "hev-fsh-client.h"

#include "hev-main.h"

static HevFshBase *instance;

static void
show_help (void)
{
    fprintf (stderr,
             "Common: [-4 | -6] [-t TIMEOUT] [-l LOG] [-v]\n"
             "Server: -s [SERVER_ADDR:SERVER_PORT]\n"
             "Terminal:\n"
             "  Forwarder: -f [-u USER] SERVER_ADDR[:SERVER_PORT/TOKEN]\n"
             "  Connector: SERVER_ADDR[:SERVER_PORT]/TOKEN\n"
             "TCP Port:\n"
             "  Forwarder: -f -p [-w ADDR:PORT,... | -b ADDR:PORT,...] "
             "SERVER_ADDR[:SERVER_PORT/TOKEN]\n"
             "  Connector: -p [LOCAL_ADDR:]LOCAL_PORT:REMOTE_ADD:REMOTE_PORT "
             "SERVER_ADDR[:SERVER_PORT]/TOKEN\n"
             "             -p REMOTE_ADD:REMOTE_PORT "
             "SERVER_ADDR[:SERVER_PORT]/TOKEN\n");
    fprintf (stderr, "Version: %d.%d.%d\n", MAJOR_VERSION, MINOR_VERSION,
             MICRO_VERSION);
}

static char *
parse_addr (const char *str, const char **addr, const char **port,
            const char **token)
{
    char *b;
    int i = 0, s = 0;

    b = strdup (str);
    if (!b)
        return NULL;

    /* parse state machine */
    for (;;) {
        switch (b[i]) {
        case '\0':
            goto exit;
        case ',':
            b[i] = '\0';
            goto exit;
        case ':':
            if (s != 1 && s != 4 && s != 5 && s != 6)
                goto error;
            if (s == 1) {
                b[i++] = '\0';
                *port = &b[i];
                s = 2;
            } else if (s == 6) {
                b[i++] = '\0';
                *port = &b[i];
                s = 7;
            } else {
                s = 5;
                i++;
            }
            break;
        case '/':
            if (s != 1 && s != 2 && s != 6 && s != 7)
                goto error;
            b[i++] = '\0';
            if (token)
                *token = &b[i];
            break;
        case '[':
            if (s != 0)
                goto error;
            *addr = &b[++i];
            s = 4;
            break;
        case ']':
            if (s != 5)
                goto error;
            b[i++] = '\0';
            s = 6;
            break;
        default:
            if (s == 0) {
                *addr = &b[i++];
                s = 1;
            } else if (s == 4) {
                s = 5;
            } else {
                i++;
            }
            break;
        }
    }

exit:
    return b;

error:
    free (b);
    return NULL;
}

static int
set_addr_list (HevFshConfig *config, const char *str, int action)
{
    const char *addr = NULL;
    const char *port = NULL;
    char iaddr[16] = { 0 };
    int nport;
    char *b;

    b = parse_addr (str, &addr, &port, NULL);
    if (!b || !addr || !port) {
        free (b);
        return -1;
    }

    nport = htons (atoi (port));
    if (inet_pton (AF_INET, addr, &iaddr[12]) == 1) {
        ((uint16_t *)iaddr)[5] = 0xffff;
        hev_fsh_config_addr_list_add (config, 4, &iaddr[12], nport, action);
        hev_fsh_config_addr_list_add (config, 6, iaddr, nport, action);
    } else {
        if (inet_pton (AF_INET6, addr, iaddr) != 1) {
            free (b);
            return -1;
        }
        hev_fsh_config_addr_list_add (config, 6, iaddr, nport, action);
        if (IN6_IS_ADDR_V4MAPPED ((struct in6_addr *)iaddr))
            hev_fsh_config_addr_list_add (config, 4, &iaddr[12], nport, action);
    }

    return 0;
}

static int
parse_set_addr_list (HevFshConfig *config, const char *str, int action)
{
    int s = 0;

    /* parse state machine */
    for (;;) {
        switch (*str) {
        case '\0':
            return 0;
        case ',':
            str++;
            s = 0;
            break;
        default:
            if (s == 0) {
                if (set_addr_list (config, str, action) < 0)
                    return -1;
                s = 1;
            } else {
                str++;
            }
            break;
        }
    }

    return 0;
}

static char *
parse_addr_pair (const char *str, const char *ps[4])
{
    char *b;
    int i = 0, j = 0, s = 0;

    b = strdup (str);
    if (!b)
        return NULL;

    /* parse state machine */
    for (;;) {
        switch (b[i]) {
        case '\0':
            goto exit;
        case ':':
            if (s != 1 && s != 2 && s != 3)
                goto error;
            if (s == 1 || s == 3) {
                b[i++] = '\0';
                if (j < 4)
                    s = 0;
            } else {
                i++;
            }
            break;
        case '[':
            if (s != 0)
                goto error;
            ps[j++] = &b[++i];
            s = 2;
            break;
        case ']':
            if (s != 2)
                goto error;
            b[i++] = '\0';
            s = 3;
            break;
        default:
            if (s == 0) {
                ps[j++] = &b[i++];
                s = 1;
            } else {
                i++;
            }
            break;
        }
    }

exit:
    return b;

error:
    free (b);
    return NULL;
}

static int
parse_set_addr_pair (HevFshConfig *config, const char *str)
{
    const char *ps[4] = { 0 };
    char *b;

    b = parse_addr_pair (str, ps);
    if (!b || !ps[0] || !ps[1]) {
        free (b);
        return -1;
    }

    if (ps[2] && ps[3]) {
        hev_fsh_config_set_local_address (config, ps[0]);
        hev_fsh_config_set_local_port (config, atoi (ps[1]));
        hev_fsh_config_set_remote_address (config, ps[2]);
        hev_fsh_config_set_remote_port (config, atoi (ps[3]));
    } else if (ps[2]) {
        hev_fsh_config_set_local_port (config, atoi (ps[0]));
        hev_fsh_config_set_remote_address (config, ps[1]);
        hev_fsh_config_set_remote_port (config, atoi (ps[2]));
    } else {
        hev_fsh_config_set_remote_address (config, ps[0]);
        hev_fsh_config_set_remote_port (config, atoi (ps[1]));
    }

    return 0;
}

static int
parse_server (HevFshConfig *config, const char *sa)
{
    if (sa) {
        const char *addr = NULL;
        const char *port = NULL;

        if (!parse_addr (sa, &addr, &port, NULL))
            return -1;

        hev_fsh_config_set_server_address (config, addr);
        if (port)
            hev_fsh_config_set_server_port (config, port);
    }

    hev_fsh_config_set_mode (config, HEV_FSH_CONFIG_MODE_SERVER);

    return 0;
}

static int
parse_client (HevFshConfig *config, int f, int p, const char *t1,
              const char *t2, const char *w, const char *b, const char *u)
{
    const char *addr = NULL;
    const char *port = NULL;
    const char *token = NULL;
    const char *ap = NULL;
    const char *sa;
    int mode;

    if (!f && p) {
        ap = t1;
        sa = t2;
    } else {
        sa = t1;
    }

    if (!sa || !parse_addr (sa, &addr, &port, &token))
        return -1;

    hev_fsh_config_set_server_address (config, addr);
    if (port)
        hev_fsh_config_set_server_port (config, port);
    hev_fsh_config_set_token (config, token);

    if (f) {
        if (p) {
            if (w && b)
                return -1;

            if (w) {
                hev_fsh_config_addr_list_add (config, 0, NULL, 0, 0);
                if (parse_set_addr_list (config, w, 1) < 0)
                    return -1;
            } else if (b) {
                hev_fsh_config_addr_list_add (config, 0, NULL, 0, 1);
                if (parse_set_addr_list (config, b, 0) < 0)
                    return -1;
            } else {
                hev_fsh_config_addr_list_add (config, 0, NULL, 0, 1);
            }
            mode = HEV_FSH_CONFIG_MODE_FORWARDER_PORT;
        } else {
            hev_fsh_config_set_user (config, u);
            mode = HEV_FSH_CONFIG_MODE_FORWARDER_TERM;
        }
    } else {
        if (!token)
            return -1;

        if (p) {
            if (parse_set_addr_pair (config, ap) < 0)
                return -1;
            mode = HEV_FSH_CONFIG_MODE_CONNECTOR_PORT;
        } else {
            mode = HEV_FSH_CONFIG_MODE_CONNECTOR_TERM;
        }
    }

    hev_fsh_config_set_mode (config, mode);

    return 0;
}

static int
parse_args (HevFshConfig *config, int argc, char *argv[])
{
    int opt;
    int v = 0;
    int s = 0;
    int f = 0;
    int p = 0;
    const char *l = NULL;
    const char *u = NULL;
    const char *w = NULL;
    const char *b = NULL;
    const char *t1 = NULL;
    const char *t2 = NULL;

    while ((opt = getopt (argc, argv, "46t:vsfpl:u:w:b:")) != -1) {
        switch (opt) {
        case '4':
            hev_fsh_config_set_ip_type (config, 4);
            break;
        case '6':
            hev_fsh_config_set_ip_type (config, 6);
            break;
        case 't':
            hev_fsh_config_set_timeout (config, strtoul (optarg, NULL, 10));
            break;
        case 'v':
            v = 1;
            break;
        case 's':
            s = 1;
            break;
        case 'f':
            f = 1;
            break;
        case 'p':
            p = 1;
            break;
        case 'l':
            l = optarg;
            break;
        case 'u':
            u = optarg;
            break;
        case 'w':
            w = optarg;
            break;
        case 'b':
            b = optarg;
            break;
        default:
            return -1;
        }
    }

    if (optind < argc)
        t1 = argv[optind++];
    if (optind < argc)
        t2 = argv[optind++];

    if (s) {
        if (parse_server (config, t1) < 0)
            return -1;
    } else {
        if (parse_client (config, f, p, t1, t2, w, b, u) < 0)
            return -1;
    }

    hev_fsh_config_set_log_path (config, l);
    if (v)
        hev_fsh_config_set_log_level (config, HEV_LOGGER_DEBUG);
    else
        hev_fsh_config_set_log_level (config, HEV_LOGGER_INFO);

    return 0;
}

static void
signal_handler (int signum)
{
    if (instance)
        hev_fsh_base_stop (instance);
}

int
main (int argc, char *argv[])
{
    HevFshConfig *config = NULL;
    int mode;
    int level;
    const char *path;

    if (hev_task_system_init () < 0)
        return -1;

    config = hev_fsh_config_new ();
    if (!config)
        return -1;

    if (parse_args (config, argc, argv) < 0) {
        show_help ();
        return -1;
    }

    mode = hev_fsh_config_get_mode (config);
    path = hev_fsh_config_get_log_path (config);
    level = hev_fsh_config_get_log_level (config);

    if (hev_logger_init (level, path) < 0)
        return -1;

    if (signal (SIGPIPE, SIG_IGN) == SIG_ERR)
        return -1;
    if (signal (SIGINT, signal_handler) == SIG_ERR)
        return -1;
    if (signal (SIGTERM, signal_handler) == SIG_ERR)
        return -1;

    if (HEV_FSH_CONFIG_MODE_SERVER == mode)
        instance = hev_fsh_server_new (config);
    else
        instance = hev_fsh_client_new (config);

    if (!instance)
        return -1;

    hev_fsh_base_start (instance);

    hev_task_system_run ();

    hev_object_unref (HEV_OBJECT (instance));
    hev_fsh_config_destroy (config);
    hev_task_system_fini ();
    hev_logger_fini ();

    return 0;
}
