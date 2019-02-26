/*
 ============================================================================
 Name        : hev-main.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 everyone.
 Description : Main
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "hev-main.h"
#include "hev-task-system.h"
#include "hev-fsh-config.h"
#include "hev-fsh-server.h"
#include "hev-fsh-client-forward.h"
#include "hev-fsh-client-term-connect.h"
#include "hev-fsh-client-port-connect.h"
#include "hev-fsh-client-port-listen.h"

#define MAJOR_VERSION (3)
#define MINOR_VERSION (6)
#define MICRO_VERSION (8)

static void
show_help (void)
{
    fprintf (stderr,
             "Server: -s [-l LOG] [SERVER_ADDR:SERVER_PORT]\n"
             "Terminal:\n"
             "  Forwarder: -f [-u USER] SERVER_ADDR[:SERVER_PORT/TOKEN]\n"
             "  Connector: SERVER_ADDR[:SERVER_PORT]/TOKEN\n"
             "TCP Port:\n"
             "  Forwarder: -f -p [-w ADDR:PORT,... | -b ADDR:PORT,...] "
             "SERVER_ADDR[:SERVER_PORT/TOKEN\n"
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
    char *saveptr;

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
    char *saveptr1;

    for (;; str = NULL) {
        char *ap;
        char *addr;
        char *port;
        char *saveptr2;
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
    char *saveptr;
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

    while ((opt = getopt (argc, argv, "sfpl:u:w:b:")) != -1) {
        switch (opt) {
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

    config = hev_fsh_config_new ();
    if (!config)
        return -1;

    if (0 > parse_args (config, argc, argv)) {
        show_help ();
        return -1;
    }

    mode = hev_fsh_config_get_mode (config);
    log = hev_fsh_config_get_log (config);

    if (0 > setup_log (mode, log))
        return -1;

    if (SIG_ERR == signal (SIGPIPE, SIG_IGN)) {
        fprintf (stderr, "Set signal pipe's handler failed!\n");
        return -1;
    }

    if (0 > hev_task_system_init ()) {
        fprintf (stderr, "Init task system failed!\n");
        return -1;
    }

    if (HEV_FSH_CONFIG_MODE_SERVER == mode) {
        HevFshServer *server;

        server = hev_fsh_server_new (config);
        if (!server) {
            fprintf (stderr, "Create fsh server failed!\n");
            return -1;
        }

        hev_fsh_server_start (server);

        hev_task_system_run ();

        hev_fsh_server_destroy (server);
    } else {
        HevFshClientBase *client;

        if (HEV_FSH_CONFIG_MODE_FORWARDER & mode) {
            for (;;) {
                client = hev_fsh_client_forward_new (config);

                hev_task_system_run ();

                hev_fsh_client_base_destroy (client);

                sleep (1);
            }
        } else if (HEV_FSH_CONFIG_MODE_CONNECTOR & mode) {
            if (HEV_FSH_CONFIG_MODE_CONNECTOR_PORT == mode) {
                if (hev_fsh_config_get_local_port (config))
                    client = hev_fsh_client_port_listen_new (config);
                else
                    client = hev_fsh_client_port_connect_new (config, -1);
            } else {
                client = hev_fsh_client_term_connect_new (config);
            }

            hev_task_system_run ();

            hev_fsh_client_base_destroy (client);
        }
    }

    hev_task_system_fini ();
    hev_fsh_config_destroy (config);

    return 0;
}
