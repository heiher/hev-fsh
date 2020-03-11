/*
 ============================================================================
 Name        : hev-fsh-client-base.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2019 everyone.
 Description : Fsh client base
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_BASE_H__
#define __HEV_FSH_CLIENT_BASE_H__

#include <hev-task.h>

#include <sys/types.h>
#include <sys/socket.h>

typedef struct _HevFshClientBase HevFshClientBase;
typedef void (*HevFshClientBaseDestroy) (HevFshClientBase *self);

struct _HevFshClientBase
{
    int fd;
    unsigned int timeout;
    struct sockaddr address;

    /* private */
    HevFshClientBaseDestroy _destroy;
};

int hev_fsh_client_base_construct (HevFshClientBase *self, const char *address,
                                   unsigned int port, unsigned int timeout);

void hev_fsh_client_base_destroy (HevFshClientBase *self);

int hev_fsh_client_base_task_io_yielder (HevTaskYieldType type, void *data);

#endif /* __HEV_FSH_CLIENT_BASE_H__ */
