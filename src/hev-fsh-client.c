/*
 ============================================================================
 Name        : hev-fsh-client.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 everyone.
 Description : Fsh client
 ============================================================================
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>
#include <pwd.h>
#include <pty.h>

#include "hev-fsh-client.h"
#include "hev-fsh-protocol.h"
#include "hev-memory-allocator.h"
#include "hev-task.h"
#include "hev-task-io-socket.h"

#define TASK_STACK_SIZE	(64 * 4096)
#define KEEP_ALIVE_INTERVAL	(30 * 1000)

typedef struct _HevFshClientAccept HevFshClientAccept;

struct _HevFshClient
{
	int fd;

	const char *user;
	const char *token;

	HevTask *task;
	struct sockaddr address;
};

struct _HevFshClientAccept
{
	const char *user;
	HevFshToken token;

	struct sockaddr address;
};

static void hev_fsh_client_accept_task_entry (void *data);
static void hev_fsh_client_forward_task_entry (void *data);
static void hev_fsh_client_connect_task_entry (void *data);

static int
hev_fsh_client_socket (void)
{
	int fd, flags;

	fd = socket (AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		return -1;

	if (fcntl (fd, F_SETFL, O_NONBLOCK) == -1) {
		close (fd);
		return -1;
	}

	flags = fcntl (fd, F_GETFD);
	if (flags == -1) {
		close (fd);
		return -1;
	}

	flags |= FD_CLOEXEC;
	if (fcntl (fd, F_SETFD, flags) == -1) {
		close (fd);
		return -1;
	}

	return fd;
}

static HevFshClient *
hev_fsh_client_new (const char *address, unsigned int port)
{
	HevFshClient *self;
	struct sockaddr_in *addr;

	self = hev_malloc0 (sizeof (HevFshClient));
	if (!self) {
		fprintf (stderr, "Allocate client failed!\n");
		return NULL;
	}

	self->fd = hev_fsh_client_socket ();
	if (self->fd == -1) {
		fprintf (stderr, "Create client's socket failed!\n");
		hev_free (self);
		return NULL;
	}

	self->task = hev_task_new (TASK_STACK_SIZE);
	if (!self->task) {
		fprintf (stderr, "Create client's task failed!\n");
		hev_free (self);
		return NULL;
	}

	addr = (struct sockaddr_in *) &self->address;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = inet_addr (address);
	addr->sin_port = htons (port);

	signal (SIGCHLD, SIG_IGN);

	return self;
}

HevFshClient *
hev_fsh_client_new_forward (const char *address, unsigned int port,
			const char *user)
{
	HevFshClient *self;

	self = hev_fsh_client_new (address, port);
	if (!self)
		return NULL;

	self->user = user;
	hev_task_run (self->task, hev_fsh_client_forward_task_entry, self);

	return self;
}

HevFshClient *
hev_fsh_client_new_connect (const char *address, unsigned int port,
			const char *token)
{
	HevFshClient *self;

	self = hev_fsh_client_new (address, port);
	if (!self)
		return NULL;

	self->token = token;
	hev_task_run (self->task, hev_fsh_client_connect_task_entry, self);

	return self;
}

void
hev_fsh_client_destroy (HevFshClient *self)
{
	close (self->fd);
	hev_free (self);
}

static void
hev_fsh_client_accept_task_entry (void *data)
{
	HevTask *task = hev_task_self ();
	HevFshClientAccept *accept = data;
	HevFshMessage message;
	HevFshMessageToken message_token;
	HevFshMessageTermInfo message_term_info;
	int sock_fd, ptm_fd;
	struct msghdr mh;
	struct iovec iov[2];
	struct winsize win_size;
	ssize_t len;
	pid_t pid;

	sock_fd = hev_fsh_client_socket ();
	if (sock_fd == -1)
		goto quit;

	hev_task_add_fd (task, sock_fd, EPOLLIN | EPOLLOUT);

	if (hev_task_io_socket_connect (sock_fd, &accept->address, sizeof (struct sockaddr_in),
					NULL, NULL) < 0)
		goto quit_close_fd;

	message.ver = 1;
	message.cmd = HEV_FSH_CMD_ACCEPT;
	memcpy (message_token.token, accept->token, sizeof (HevFshToken));

	memset (&mh, 0, sizeof (mh));
	mh.msg_iov = iov;
	mh.msg_iovlen = 2;

	iov[0].iov_base = &message;
	iov[0].iov_len = sizeof (message);
	iov[1].iov_base = &message_token;
	iov[1].iov_len = sizeof (message_token);

	if (hev_task_io_socket_sendmsg (sock_fd, &mh, MSG_WAITALL, NULL, NULL) <= 0)
		goto quit_close_fd;

	/* recv message term info */
	len = hev_task_io_socket_recv (sock_fd, &message_term_info, sizeof (message_term_info),
				MSG_WAITALL, NULL, NULL);
	if (len <= 0)
		return;

	pid = forkpty (&ptm_fd, NULL, NULL, NULL);
	if (pid == -1) {
		goto quit_close_fd;
	} else if (pid == 0) {
		const char *sh = "/bin/sh";
		const char *bash = "/bin/bash";
		const char *cmd = bash;

		if (access (bash, X_OK) < 0)
			cmd = sh;

		if (getuid () == 0) {
			if (accept->user) {
				struct passwd *pwd;

				pwd = getpwnam (accept->user);
				if (pwd) {
					setgid (pwd->pw_gid);
					setuid (pwd->pw_uid);
				}
			} else {
				setsid ();
				cmd = "/bin/login";
			}
		}

		if (!getenv ("TERM"))
			setenv ("TERM", "linux", 1);

		execl (cmd, cmd, NULL);

		exit (0);
	} else {
		win_size.ws_row = message_term_info.rows;
		win_size.ws_col = message_term_info.columns;
		win_size.ws_xpixel = 0;
		win_size.ws_ypixel = 0;

		ioctl (ptm_fd, TIOCSWINSZ, &win_size);
	}

	if (fcntl (ptm_fd, F_SETFL, O_NONBLOCK) == -1)
		goto quit_close_all_fd;

	hev_task_add_fd (task, ptm_fd, EPOLLIN | EPOLLOUT);

	hev_task_io_splice (sock_fd, sock_fd, ptm_fd, ptm_fd, 2048, NULL, NULL);

quit_close_all_fd:
	close (ptm_fd);
quit_close_fd:
	close (sock_fd);
quit:
	hev_free (accept);
}

static void
hev_fsh_client_forward_task_entry (void *data)
{
	HevTask *task = hev_task_self ();
	HevFshClient *self = data;
	HevFshMessage message;
	HevFshMessageToken message_token;
	HevFshToken token;
	ssize_t len;
	char token_str[40];

	hev_task_add_fd (task, self->fd, EPOLLIN | EPOLLOUT);

	if (hev_task_io_socket_connect (self->fd, &self->address, sizeof (struct sockaddr_in),
					NULL, NULL) < 0)
	{
		fprintf (stderr, "Connect to server failed!\n");
		return;
	}

	message.ver = 1;
	message.cmd = HEV_FSH_CMD_LOGIN;

	len = hev_task_io_socket_send (self->fd, &message, sizeof (message), MSG_WAITALL,
				NULL, NULL);
	if (len <= 0)
		return;

	/* recv message token */
	len = hev_task_io_socket_recv (self->fd, &message, sizeof (message), MSG_WAITALL,
				NULL, NULL);
	if (len <= 0)
		return;

	if (message.cmd != HEV_FSH_CMD_TOKEN) {
		fprintf (stderr, "Can't login to server!\n");
		return;
	}

	len = hev_task_io_socket_recv (self->fd, &message_token, sizeof (message_token),
				MSG_WAITALL, NULL, NULL);
	if (len <= 0)
		return;

	memcpy (token, message_token.token, sizeof (HevFshToken));
	hev_fsh_protocol_token_to_string (token, token_str);
	printf ("Token: %s\n", token_str);

	for (;;) {
		HevFshClientAccept *accept;

		hev_task_sleep (KEEP_ALIVE_INTERVAL);

		len = recv (self->fd, &message, sizeof (message), MSG_PEEK);
		if (len == -1 && errno == EAGAIN) {
			/* keep alive */
			message.ver = 1;
			message.cmd = HEV_FSH_CMD_KEEP_ALIVE;
			len = hev_task_io_socket_send (self->fd, &message, sizeof (message),
						MSG_WAITALL, NULL, NULL);
			if (len <= 0)
				return;
			continue;
		}

		/* recv message connect */
		len = hev_task_io_socket_recv (self->fd, &message, sizeof (message), MSG_WAITALL,
					NULL, NULL);
		if (len <= 0)
			return;

		if (message.cmd != HEV_FSH_CMD_CONNECT)
			return;

		len = hev_task_io_socket_recv (self->fd, &message_token, sizeof (message_token),
					MSG_WAITALL, NULL, NULL);
		if (len <= 0)
			return;

		if (memcmp (message_token.token, token, sizeof (HevFshToken)) != 0)
			return;

		accept = hev_malloc0 (sizeof (HevFshClientAccept));
		if (!accept)
			continue;

		accept->user = self->user;
		memcpy (accept->token, message_token.token, sizeof (HevFshToken));
		memcpy (&accept->address, &self->address, sizeof (struct sockaddr_in));

		task = hev_task_new (TASK_STACK_SIZE);
		hev_task_run (task, hev_fsh_client_accept_task_entry, accept);
	}
}

static void
hev_fsh_client_connect_task_entry (void *data)
{
	HevTask *task = hev_task_self ();
	HevFshClient *self = data;
	HevFshMessage message;
	HevFshMessageToken message_token;
	HevFshMessageTermInfo message_term_info;
	struct termios term, term_rsh;
	struct winsize win_size;
	ssize_t len;

	hev_task_add_fd (task, self->fd, EPOLLIN | EPOLLOUT);

	if (hev_task_io_socket_connect (self->fd, &self->address, sizeof (struct sockaddr_in),
					NULL, NULL) < 0)
	{
		fprintf (stderr, "Connect to server failed!\n");
		return;
	}

	message.ver = 1;
	message.cmd = HEV_FSH_CMD_CONNECT;

	len = hev_task_io_socket_send (self->fd, &message, sizeof (message), MSG_WAITALL,
				NULL, NULL);
	if (len <= 0)
		return;

	if (hev_fsh_protocol_token_from_string (message_token.token, self->token) == -1) {
		fprintf (stderr, "Can't parse token!\n");
		return;
	}

	/* send message token */
	len = hev_task_io_socket_send (self->fd, &message_token, sizeof (message_token),
				MSG_WAITALL, NULL, NULL);
	if (len <= 0)
		return;

	if (ioctl (0, TIOCGWINSZ, &win_size) < 0)
		return;

	message_term_info.rows = win_size.ws_row;
	message_term_info.columns = win_size.ws_col;

	/* send message token */
	len = hev_task_io_socket_send (self->fd, &message_term_info, sizeof (message_term_info),
				MSG_WAITALL, NULL, NULL);
	if (len <= 0)
		return;

	if (fcntl (0, F_SETFL, O_NONBLOCK) == -1)
		return;
	if (fcntl (1, F_SETFL, O_NONBLOCK) == -1)
		return;

	hev_task_add_fd (task, 0, EPOLLIN);
	hev_task_add_fd (task, 1, EPOLLOUT);

	if (tcgetattr (0, &term) == -1)
		return;

	memcpy (&term_rsh, &term, sizeof (term));
	term_rsh.c_oflag &= ~(OPOST);
	term_rsh.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK);
	if (tcsetattr (0, TCSADRAIN, &term_rsh) == -1)
		return;

	hev_task_io_splice (self->fd, self->fd, 0, 1, 2048, NULL, NULL);

	tcsetattr (0, TCSADRAIN, &term);
}
