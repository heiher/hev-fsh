/*
 ============================================================================
 Name        : hev-main.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2020 everyone.
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
#include <arpa/inet.h>

#include <hev-task-system.h>

#include "hev-fsh-config.h"
#include "hev-fsh-server.h"
#include "hev-fsh-client.h"

#include "hev-main.h"

static HevFshBase *instance;

static void
show_help (void)
{
    fprintf (stderr,
             "Common: [-t TIMEOUT]\n"
             "Server: -s [-l LOG] [SERVER_ADDR:SERVER_PORT]\n"
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

static void
parse_addr (const char *str, char **addr, char **port, char **token)
{
    char *saveptr = NULL;

    *addr = strtok_r ((char *)str, "/", &saveptr);
    if (token)
        *token = strtok_r (NULL, "/", &saveptr);
    if (*addr) {
        *addr = strtok_r (*addr, ":", &saveptr);
        *port = strtok_r (NULL, ":", &saveptr);
    }
}

static void
parse_addr_list (HevFshConfig *config, const char *str, int action)
{
    char *saveptr1 = NULL;

    for (;; str = NULL) {
        char *ap;
        char *addr;
        char *port;
        char *saveptr2 = NULL;
        struct in_addr iaddr;

        ap = strtok_r ((char *)str, ",", &saveptr1);
        if (!ap)
            break;

        addr = strtok_r (ap, ":", &saveptr2);
        port = strtok_r (NULL, ":", &saveptr2);
        if (!addr || !port)
            continue;

        if (!inet_aton (addr, &iaddr))
            continue;

        hev_fsh_config_addr_list_add (config, 4, &iaddr, htons (atoi (port)),
                                      action);
    }
}

static int
parse_addr_pair (HevFshConfig *config, const char *str)
{
    char *saveptr = NULL;
    char *v1;
    char *v2;
    char *v3;
    char *v4;

    v1 = strtok_r ((char *)str, ":", &saveptr);
    v2 = strtok_r (NULL, ":", &saveptr);
    v3 = strtok_r (NULL, ":", &saveptr);
    v4 = strtok_r (NULL, ":", &saveptr);

    if (!v1 || !v2)
        return -1;

    if (v3 && v4) {
        hev_fsh_config_set_local_address (config, v1);
        hev_fsh_config_set_local_port (config, atoi (v2));
        hev_fsh_config_set_remote_address (config, v3);
        hev_fsh_config_set_remote_port (config, atoi (v4));
    } else if (v3) {
        hev_fsh_config_set_local_port (config, atoi (v1));
        hev_fsh_config_set_remote_address (config, v2);
        hev_fsh_config_set_remote_port (config, atoi (v3));
    } else {
        hev_fsh_config_set_remote_address (config, v1);
        hev_fsh_config_set_remote_port (config, atoi (v2));
    }

    return 0;
}

static int
parse_args (HevFshConfig *config, int argc, char *argv[])
{
    int opt;
    int mode;
    int s = 0;
    int f = 0;
    int p = 0;
    const char *l = NULL;
    const char *u = NULL;
    const char *w = NULL;
    const char *b = NULL;
    const char *t1 = NULL;
    const char *t2 = NULL;

    while ((opt = getopt (argc, argv, "t:sfpl:u:w:b:")) != -1) {
        switch (opt) {
        case 't':
            hev_fsh_config_set_timeout (config, strtoul (optarg, NULL, 10));
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
        const char *sa = t1;

        if (sa) {
            char *addr;
            char *port;

            parse_addr (sa, &addr, &port, NULL);
            hev_fsh_config_set_server_address (config, addr);
            if (port)
                hev_fsh_config_set_server_port (config, atoi (port));
        }

        hev_fsh_config_set_log (config, l);
        mode = HEV_FSH_CONFIG_MODE_SERVER;
    } else {
        char *addr;
        char *port;
        char *token;
        const char *sa;
        const char *ap = NULL;

        if (!f && p) {
            ap = t1;
            sa = t2;
        } else {
            sa = t1;
        }

        if (!sa)
            return -1;

        parse_addr (sa, &addr, &port, &token);
        hev_fsh_config_set_server_address (config, addr);
        if (port)
            hev_fsh_config_set_server_port (config, atoi (port));
        hev_fsh_config_set_token (config, token);

        if (f) {
            if (p) {
                if (w && b)
                    return -1;

                if (w) {
                    hev_fsh_config_addr_list_add (config, 0, NULL, 0, 0);
                    parse_addr_list (config, w, 1);
                } else if (b) {
                    hev_fsh_config_addr_list_add (config, 0, NULL, 0, 1);
                    parse_addr_list (config, b, 0);
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
                if (0 > parse_addr_pair (config, ap))
                    return -1;
                mode = HEV_FSH_CONFIG_MODE_CONNECTOR_PORT;
            } else {
                mode = HEV_FSH_CONFIG_MODE_CONNECTOR_TERM;
            }
        }
    }

    hev_fsh_config_set_mode (config, mode);

    return 0;
}

static void
signal_handler (int signum)
{
    if (instance)
        hev_fsh_base_stop (instance);
}

static int
setup_log (int mode, const char *log)
{
    int fd;

    if (HEV_FSH_CONFIG_MODE_SERVER != mode || !log)
        return 0;

    /* redirect log */
    fd = open (log, O_CREAT | O_APPEND | O_WRONLY, 0644);
    if (fd == -1) {
        fprintf (stderr, "Open log file %s failed!\n", log);
        return -1;
    }

    close (1);
    close (2);
    dup2 (fd, 1);
    dup2 (fd, 2);

    return 0;
}

int
main (int argc, char *argv[])
{
    HevFshConfig *config = NULL;
    int mode;
    const char *log;

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
    log = hev_fsh_config_get_log (config);

    if (setup_log (mode, log) < 0)
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

    hev_fsh_base_destroy (instance);
    hev_fsh_config_destroy (config);
    hev_task_system_fini ();

    return 0;
}
