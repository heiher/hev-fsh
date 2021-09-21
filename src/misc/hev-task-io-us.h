/*
 ============================================================================
 Name        : hev-task-io-us.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Task I/O operations
 ============================================================================
 */

#ifndef __HEV_TASK_IO_US_H__
#define __HEV_TASK_IO_US_H__

#ifdef __cplusplus
extern "C" {
#endif

void hev_task_io_us_splice (int fd_a_i, int fd_a_o, int fd_b_i, int fd_b_o,
                            size_t buf_size, HevTaskIOYielder yielder,
                            void *yielder_data);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_TASK_IO_US_H__ */
