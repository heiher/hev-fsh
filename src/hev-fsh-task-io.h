/*
 ============================================================================
 Name        : hev-fsh-task-io.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 everyone.
 Description : Fsh task I/O operations
 ============================================================================
 */

#ifndef __HEV_FSH_TASK_IO_H__
#define __HEV_FSH_TASK_IO_H__

#include "hev-task.h"

typedef int (*HevFshTaskIOYielder) (HevTaskYieldType type, void *data);

int hev_fsh_task_io_socket_connect (int fd, const struct sockaddr *addr, socklen_t addr_len,
			HevFshTaskIOYielder yielder, void *yielder_data);

int hev_fsh_task_io_socket_accept (int fd, struct sockaddr *addr, socklen_t *addr_len,
			HevFshTaskIOYielder yielder, void *yielder_data);

ssize_t hev_fsh_task_io_socket_recv (int fd, void *buf, size_t len, int flags,
			HevFshTaskIOYielder yielder, void *yielder_data);

ssize_t hev_fsh_task_io_socket_send (int fd, const void *buf, size_t len, int flags,
			HevFshTaskIOYielder yielder, void *yielder_data);

ssize_t hev_fsh_task_io_socket_sendmsg (int fd, const struct msghdr *msg, int flags,
			HevFshTaskIOYielder yielder, void *yielder_data);

void hev_fsh_task_io_splice (int fd_a_i, int fd_a_o, int fd_b_i, int fd_b_o, size_t buf_size,
			HevFshTaskIOYielder yielder, void *yielder_data);

#endif /* __HEV_FSH_TASK_IO_H__ */

