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
#include "hev-fsh-client.h"

#define MAJOR_VERSION	(1)
#define MINOR_VERSION	(0)
#define MICRO_VERSION	(0)

static void
show_help (void)
{
	fprintf (stderr, "\tServer: [-a ADDRESS] [-p PORT] [-l LOG]\n");
	fprintf (stderr, "\tClient Forward: -s ADDRESS [-p PORT] [-u USER]\n");
	fprintf (stderr, "\tClient Connect: -s ADDRESS [-p PORT] -c TOKEN\n");
	fprintf (stderr, "Version: %d.%d.%d\n", MAJOR_VERSION, MINOR_VERSION, MICRO_VERSION);
}

int
main (int argc, char *argv[])
{
	int opt;
	const char *listen_address = "0.0.0.0";
	const char *server_address = NULL;
	unsigned int port = 6339;
	const char *log = NULL;
	const char *user = NULL;
	const char *token = NULL;

	while ((opt = getopt(argc, argv, "a:p:s:l:u:c:")) != -1) {
		switch (opt) {
		case 'a':
			listen_address = optarg;
			break;
		case 's':
			server_address = optarg;
			break;
		case 'p':
			port = atoi (optarg);
			break;
		case 'l':
			log = optarg;
			break;
		case 'u':
			user = optarg;
			break;
		case 'c':
			token = optarg;
			break;
		default: /* '?' */
			show_help ();
			return -1;
		}
	}

	/* redirect log for server */
	if (log && !server_address) {
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

	if (!server_address) {
		HevFshServer *server;

		server = hev_fsh_server_new (listen_address, port);
		if (!server) {
			fprintf (stderr, "Create fsh server failed!\n");
			return -1;
		}

		hev_fsh_server_start (server);

		hev_task_system_run ();

		hev_fsh_server_destroy (server);
	} else {
		HevFshClient *client;

		if (token)
			client = hev_fsh_client_new_connect (server_address, port, token);
		else
			client = hev_fsh_client_new_forward (server_address, port, user);

		hev_task_system_run ();

		hev_fsh_client_destroy (client);
	}

	hev_task_system_fini ();

	return 0;
}

