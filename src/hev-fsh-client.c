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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>
#include <pwd.h>
#include <pty.h>

#include "hev-fsh-client.h"
#include "hev-fsh-protocol.h"
#include "hev-memory-allocator.h"
#include "hev-task.h"

#define TASK_STACK_SIZE	(64 * 4096)

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
static void signal_handler (int signum);

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

	signal (SIGCHLD, signal_handler);

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

static ssize_t
task_socket_recv (int fd, void *buf, size_t len, int flags)
{
	ssize_t s;
	size_t size = 0;

retry:
	s = recv (fd, buf + size, len - size, flags & ~MSG_WAITALL);
	if (s == -1 && errno == EAGAIN) {
		hev_task_yield (HEV_TASK_WAITIO);
		goto retry;
	}

	if (!(flags & MSG_WAITALL))
		return s;

	if (s <= 0)
		return size;

	size += s;
	if (size < len)
		goto retry;

	return size;
}

static ssize_t
task_socket_send (int fd, const void *buf, size_t len, int flags)
{
	ssize_t s;
	size_t size = 0;

retry:
	s = send (fd, buf + size, len - size, flags & ~MSG_WAITALL);
	if (s == -1 && errno == EAGAIN) {
		hev_task_yield (HEV_TASK_WAITIO);
		goto retry;
	}

	if (!(flags & MSG_WAITALL))
		return s;

	if (s <= 0)
		return size;

	size += s;
	if (size < len)
		goto retry;

	return size;
}

static ssize_t
task_socket_sendmsg (int fd, const struct msghdr *msg, int flags)
{
	ssize_t s;
	size_t i, size = 0, len = 0;
	struct msghdr mh;
	struct iovec iov[msg->msg_iovlen];

	mh.msg_name = msg->msg_name;
	mh.msg_namelen = msg->msg_namelen;
	mh.msg_control = msg->msg_control;
	mh.msg_controllen = msg->msg_controllen;
	mh.msg_flags = msg->msg_flags;
	mh.msg_iov = iov;
	mh.msg_iovlen = msg->msg_iovlen;

	for (i=0; i<msg->msg_iovlen; i++) {
		iov[i] = msg->msg_iov[i];
		len += iov[i].iov_len;
	}

retry:
	s = sendmsg (fd, &mh, flags & ~MSG_WAITALL);
	if (s == -1 && errno == EAGAIN) {
		hev_task_yield (HEV_TASK_WAITIO);
		goto retry;
	}

	if (!(flags & MSG_WAITALL))
		return s;

	if (s <= 0)
		return size;

	size += s;
	if (size < len) {
		for (i=0; i<mh.msg_iovlen; i++) {
			if (s < iov[i].iov_len) {
				iov[i].iov_base += s;
				iov[i].iov_len -= s;
				break;
			}

			s -= iov[i].iov_len;
		}

		mh.msg_iov += i;
		mh.msg_iovlen -= i;

		goto retry;
	}

	return size;
}

static int
task_socket_splice (int fd_in, int fd_out, void *buf, size_t len,
			size_t *w_off, size_t *w_left)
{
	ssize_t s;

	if (*w_left == 0) {
		s = read (fd_in, buf, len);
		if (s == -1) {
			if (errno == EAGAIN)
				return 0;
			else
				return -1;
		} else if (s == 0) {
			return -1;
		} else {
			*w_off = 0;
			*w_left = s;
		}
	}

	s = write (fd_out, buf + *w_off, *w_left);
	if (s == -1) {
		if (errno == EAGAIN)
			return 0;
		else
			return -1;
	} else if (s == 0) {
		return -1;
	} else {
		*w_off += s;
		*w_left -= s;
	}

	return *w_off;
}

static int
task_socket_connect (int fd, const struct sockaddr *addr, socklen_t addr_len)
{
	int ret;
retry:
	ret = connect (fd, addr, addr_len);
	if (ret == -1 && (errno == EINPROGRESS || errno == EALREADY)) {
		hev_task_yield (HEV_TASK_WAITIO);
		goto retry;
	}

	return ret;
}

static void
hev_fsh_client_splice (int fd_a, int fd_b_i, int fd_b_o)
{
	int splice_f = 1, splice_b = 1;
	size_t w_off_f = 0, w_off_b = 0;
	size_t w_left_f = 0, w_left_b = 0;
	unsigned char buf_f[2048];
	unsigned char buf_b[2048];

	for (;;) {
		int no_data = 0;

		if (splice_f) {
			int ret;

			ret = task_socket_splice (fd_a, fd_b_o,
						buf_f, 2048, &w_off_f, &w_left_f);
			if (ret == 0) { /* no data */
				/* forward no data and backward closed, quit */
				if (!splice_b)
					break;
				no_data ++;
			} else if (ret == -1) { /* error */
				/* forward error and backward closed, quit */
				if (!splice_b)
					break;
				/* forward error or closed, mark to skip */
				splice_f = 0;
			}
		}

		if (splice_b) {
			int ret;

			ret = task_socket_splice (fd_b_i, fd_a,
						buf_b, 2048, &w_off_b, &w_left_b);
			if (ret == 0) { /* no data */
				/* backward no data and forward closed, quit */
				if (!splice_f)
					break;
				no_data ++;
			} else if (ret == -1) { /* error */
				/* backward error and forward closed, quit */
				if (!splice_f)
					break;
				/* backward error or closed, mark to skip */
				splice_b = 0;
			}
		}

		/* single direction no data, goto yield.
		 * double direction no data, goto waitio.
		 */
		hev_task_yield ((no_data < 2) ? HEV_TASK_YIELD : HEV_TASK_WAITIO);
	}
}

static void
hev_fsh_client_accept_task_entry (void *data)
{
	HevTask *task = hev_task_self ();
	HevFshClientAccept *accept = data;
	HevFshMessage message;
	HevFshMessageToken message_token;
	int sock_fd, ptm_fd;
	struct msghdr mh;
	struct iovec iov[2];
	pid_t pid;

	sock_fd = hev_fsh_client_socket ();
	if (sock_fd == -1)
		goto quit;

	hev_task_add_fd (task, sock_fd, EPOLLIN | EPOLLOUT);

	if (task_socket_connect (sock_fd, &accept->address, sizeof (struct sockaddr_in)) < 0)
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

	if (task_socket_sendmsg (sock_fd, &mh, MSG_WAITALL) <= 0)
		goto quit_close_fd;

	pid = forkpty (&ptm_fd, NULL, NULL, NULL);
	if (pid == -1) {
		goto quit_close_fd;
	} else if (pid == 0) {
		const char *cmd = "/bin/sh";

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

		execl (cmd, cmd, NULL);

		exit (0);
	}

	if (fcntl (ptm_fd, F_SETFL, O_NONBLOCK) == -1)
		goto quit_close_all_fd;

	hev_task_add_fd (task, ptm_fd, EPOLLIN | EPOLLOUT);

	hev_fsh_client_splice (sock_fd, ptm_fd, ptm_fd);

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

	if (task_socket_connect (self->fd, &self->address, sizeof (struct sockaddr_in)) < 0) {
		fprintf (stderr, "Connect to server failed!\n");
		return;
	}

	message.ver = 1;
	message.cmd = HEV_FSH_CMD_LOGIN;

	len = task_socket_send (self->fd, &message, sizeof (message), MSG_WAITALL);
	if (len <= 0)
		return;

	/* recv message token */
	len = task_socket_recv (self->fd, &message, sizeof (message), MSG_WAITALL);
	if (len <= 0)
		return;

	if (message.cmd != HEV_FSH_CMD_TOKEN) {
		fprintf (stderr, "Can't login to server!\n");
		return;
	}

	len = task_socket_recv (self->fd, &message_token, sizeof (message_token), MSG_WAITALL);
	if (len <= 0)
		return;

	memcpy (token, message_token.token, sizeof (HevFshToken));
	hev_fsh_protocol_token_to_string (token, token_str);
	printf ("Token: %s\n", token_str);

	for (;;) {
		HevFshClientAccept *accept;

		hev_task_sleep (30 * 1000);

		len = recv (self->fd, &message, sizeof (message), MSG_PEEK);
		if (len == -1 && errno == EAGAIN) {
			/* keep alive */
			message.ver = 1;
			message.cmd = HEV_FSH_CMD_KEEP_ALIVE;
			len = task_socket_send (self->fd, &message, sizeof (message), MSG_WAITALL);
			if (len <= 0)
				return;
			continue;
		}

		/* recv message connect */
		len = task_socket_recv (self->fd, &message, sizeof (message), MSG_WAITALL);
		if (len <= 0)
			return;

		if (message.cmd != HEV_FSH_CMD_CONNECT)
			return;

		len = task_socket_recv (self->fd, &message_token, sizeof (message_token), MSG_WAITALL);
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
	struct termios term, term_rsh;
	ssize_t len;

	hev_task_add_fd (task, self->fd, EPOLLIN | EPOLLOUT);

	if (task_socket_connect (self->fd, &self->address, sizeof (struct sockaddr_in)) < 0) {
		fprintf (stderr, "Connect to server failed!\n");
		return;
	}

	message.ver = 1;
	message.cmd = HEV_FSH_CMD_CONNECT;

	len = task_socket_send (self->fd, &message, sizeof (message), MSG_WAITALL);
	if (len <= 0)
		return;

	if (hev_fsh_protocol_token_from_string (message_token.token, self->token) == -1) {
		fprintf (stderr, "Can't parse token!\n");
		return;
	}

	/* send message token */
	len = task_socket_send (self->fd, &message_token, sizeof (message_token), MSG_WAITALL);
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

	hev_fsh_client_splice (self->fd, 0, 1);

	tcsetattr (0, TCSADRAIN, &term);
}

static void
signal_handler (int signum)
{
	waitpid (-1, NULL, WNOHANG);
}
