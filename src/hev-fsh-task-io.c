/*
 ============================================================================
 Name        : hev-fsh-task-io.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 everyone.
 Description : Fsh task I/O operations
 ============================================================================
 */

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

#include "hev-fsh-task-io.h"

int
hev_fsh_task_io_socket_connect (int fd, const struct sockaddr *addr, socklen_t addr_len,
			HevFshTaskIOYielder yielder, void *yielder_data)
{
	int ret;
retry:
	ret = connect (fd, addr, addr_len);
	if (ret == -1 && (errno == EINPROGRESS || errno == EALREADY)) {
		if (yielder) {
			if (yielder (HEV_TASK_WAITIO, yielder_data))
				return -2;
		} else {
			hev_task_yield (HEV_TASK_WAITIO);
		}
		goto retry;
	}

	return ret;
}

int
hev_fsh_task_io_socket_accept (int fd, struct sockaddr *addr, socklen_t *addr_len,
			HevFshTaskIOYielder yielder, void *yielder_data)
{
	int new_fd;
retry:
	new_fd = accept (fd, addr, addr_len);
	if (new_fd == -1 && errno == EAGAIN) {
		if (yielder) {
			if (yielder (HEV_TASK_WAITIO, yielder_data))
				return -2;
		} else {
			hev_task_yield (HEV_TASK_WAITIO);
		}
		goto retry;
	}

	return new_fd;
}

ssize_t
hev_fsh_task_io_socket_recv (int fd, void *buf, size_t len, int flags,
			HevFshTaskIOYielder yielder, void *yielder_data)
{
	ssize_t s;
	size_t size = 0;

retry:
	s = recv (fd, buf + size, len - size, flags & ~MSG_WAITALL);
	if (s == -1 && errno == EAGAIN) {
		if (yielder) {
			if (yielder (HEV_TASK_WAITIO, yielder_data))
				return -2;
		} else {
			hev_task_yield (HEV_TASK_WAITIO);
		}
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

ssize_t
hev_fsh_task_io_socket_send (int fd, const void *buf, size_t len, int flags,
			HevFshTaskIOYielder yielder, void *yielder_data)
{
	ssize_t s;
	size_t size = 0;

retry:
	s = send (fd, buf + size, len - size, flags & ~MSG_WAITALL);
	if (s == -1 && errno == EAGAIN) {
		if (yielder) {
			if (yielder (HEV_TASK_WAITIO, yielder_data))
				return -2;
		} else {
			hev_task_yield (HEV_TASK_WAITIO);
		}
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

ssize_t
hev_fsh_task_io_socket_sendmsg (int fd, const struct msghdr *msg, int flags,
			HevFshTaskIOYielder yielder, void *yielder_data)
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
		if (yielder) {
			if (yielder (HEV_TASK_WAITIO, yielder_data))
				return -2;
		} else {
			hev_task_yield (HEV_TASK_WAITIO);
		}
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
task_io_splice (int fd_in, int fd_out, void *buf, size_t len,
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

void
hev_fsh_task_io_splice (int fd_a_i, int fd_a_o, int fd_b_i, int fd_b_o, size_t buf_size,
			HevFshTaskIOYielder yielder, void *yielder_data)
{
	int splice_f = 1, splice_b = 1;
	size_t w_off_f = 0, w_off_b = 0;
	size_t w_left_f = 0, w_left_b = 0;
	unsigned char buf_f[buf_size];
	unsigned char buf_b[buf_size];

	for (;;) {
		int no_data = 0;
		HevTaskYieldType type;

		if (splice_f) {
			int ret;

			ret = task_io_splice (fd_a_i, fd_b_o, buf_f, buf_size,
						&w_off_f, &w_left_f);
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

			ret = task_io_splice (fd_b_i, fd_a_o, buf_b, buf_size,
						&w_off_b, &w_left_b);
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
		type = (no_data < 2) ? HEV_TASK_YIELD : HEV_TASK_WAITIO;
		if (yielder) {
			if (yielder (type, yielder_data))
				break;
		} else {
			hev_task_yield (type);
		}
	}
}
