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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hev-main.h"
#include "hev-task-system.h"
#include "hev-fsh-server.h"
#include "hev-fsh-client-term-forward.h"
#include "hev-fsh-client-term-connect.h"
#include "hev-fsh-client-port-forward.h"
#include "hev-fsh-client-port-listen.h"

#define MAJOR_VERSION (2)
#define MINOR_VERSION (0)
#define MICRO_VERSION (0)

static void
show_help (void)
{
    fprintf (stderr, "\tServer: [-a ADDRESS] [-p PORT] [-l LOG]\n");
    fprintf (stderr, "\tClient Common: -s ADDRESS [-p PORT]\n");
    fprintf (stderr, "\t\tTerminal Forward: [-u USER] [-t TOKEN]\n");
    fprintf (stderr, "\t\tTerminal Connect: -c TOKEN\n");
    fprintf (stderr, "\t\tTCP Port Forward: -o ADDRESS -x PORT [-t TOKEN]\n");
    fprintf (stderr, "\t\tTCP Port Connect: -c TOKEN [-o ADDRESS] -x PORT\n");
    fprintf (stderr, "Version: %d.%d.%d\n", MAJOR_VERSION, MINOR_VERSION,
             MICRO_VERSION);
}

static void
server_run (const char *lis_addr, unsigned int port)
{
    HevFshServer *server;

    server = hev_fsh_server_new (lis_addr, port);
    if (!server) {
        fprintf (stderr, "Create fsh server failed!\n");
        return;
    }

    hev_fsh_server_start (server);

    hev_task_system_run ();

    hev_fsh_server_destroy (server);
}

static void
client_term_forward_run (const char *srv_addr, unsigned int port,
                         const char *user, const char *token)
{
    for (;;) {
        HevFshClientTermForward *client;

        client = hev_fsh_client_term_forward_new (srv_addr, port, user, token);

        hev_task_system_run ();

        hev_fsh_client_base_destroy ((HevFshClientBase *)client);

        sleep (1);
    }
}

static void
client_term_connect_run (const char *srv_addr, unsigned int port,
                         const char *token)
{
    HevFshClientTermConnect *client;

    client = hev_fsh_client_term_connect_new (srv_addr, port, token);

    hev_task_system_run ();

    hev_fsh_client_base_destroy ((HevFshClientBase *)client);
}

static void
client_port_forward_run (const char *srv_addr, unsigned int srv_port,
                         const char *tgt_addr, unsigned int tgt_port,
                         const char *token)
{
    for (;;) {
        HevFshClientPortForward *client;

        client = hev_fsh_client_port_forward_new (srv_addr, srv_port, tgt_addr,
                                                  tgt_port, token);

        hev_task_system_run ();

        hev_fsh_client_base_destroy ((HevFshClientBase *)client);

        sleep (1);
    }
}

static void
client_port_listen_run (const char *lis_addr, unsigned int lis_port,
                        const char *srv_addr, unsigned int srv_port,
                        const char *token)
{
    HevFshClientPortListen *client;

    client = hev_fsh_client_port_listen_new (lis_addr, lis_port, srv_addr,
                                             srv_port, token);

    hev_task_system_run ();

    hev_fsh_client_base_destroy ((HevFshClientBase *)client);
}

int
main (int argc, char *argv[])
{
    int opt;
    const char *lis_addr = "0.0.0.0";
    const char *srv_addr = NULL;
    const char *loc_addr = "127.0.0.1";
    unsigned int srv_port = 6339;
    unsigned int loc_port = 0;
    const char *cval = NULL;
    const char *log = NULL;
    const char *user = NULL;
    const char *token = NULL;

    while ((opt = getopt (argc, argv, "a:p:s:l:u:c:t:o:x:")) != -1) {
        switch (opt) {
        case 'a':
            lis_addr = optarg;
            break;
        case 's':
            srv_addr = optarg;
            break;
        case 'p':
            srv_port = atoi (optarg);
            break;
        case 'l':
            log = optarg;
            break;
        case 'u':
            user = optarg;
            break;
        case 't':
            token = optarg;
            break;
        case 'c':
            cval = optarg;
            break;
        case 'o':
            loc_addr = optarg;
            break;
        case 'x':
            loc_port = atoi (optarg);
            break;
        default: /* '?' */
            show_help ();
            return -1;
        }
    }

    /* redirect log for server */
    if (log && !srv_addr) {
        int fd;

        fd = open (log, O_CREAT | O_APPEND | O_WRONLY, 0644);
        if (fd == -1) {
            fprintf (stderr, "Open log file %s failed!\n", log);
            return -1;
        }

        close (1);
        close (2);
        dup2 (fd, 1);
        dup2 (fd, 2);
    }

    if (signal (SIGPIPE, SIG_IGN) == SIG_ERR) {
        fprintf (stderr, "Set signal pipe's handler failed!\n");
        return -1;
    }

    if (hev_task_system_init () < 0) {
        fprintf (stderr, "Init task system failed!\n");
        return -1;
    }

    if (srv_addr) {
        if (loc_port) {
            if (cval) {
                client_port_listen_run (loc_addr, loc_port, srv_addr, srv_port,
                                        cval);
            } else {
                client_port_forward_run (srv_addr, srv_port, loc_addr, loc_port,
                                         token);
            }
        } else {
            if (cval) {
                client_term_connect_run (srv_addr, srv_port, cval);
            } else {
                client_term_forward_run (srv_addr, srv_port, user, token);
            }
        }
    } else {
        server_run (lis_addr, srv_port);
    }

    hev_task_system_fini ();

    return 0;
}
