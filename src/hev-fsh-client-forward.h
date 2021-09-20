/*
 ============================================================================
 Name        : hev-fsh-client-forward.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client forward
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_FORWARD_H__
#define __HEV_FSH_CLIENT_FORWARD_H__

#include <hev-task.h>
#include <hev-task-mutex.h>

#include "hev-fsh-config.h"
#include "hev-fsh-protocol.h"
#include "hev-fsh-client-base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_FORWARD(p) ((HevFshClientForward *)p)
#define HEV_FSH_CLIENT_FORWARD_CLASS(P) ((HevFshClientForwardClass *)p)
#define HEV_FSH_CLIENT_FORWARD_TYPE (hev_fsh_client_forward_class ())

typedef struct _HevFshClientForward HevFshClientForward;
typedef struct _HevFshClientForwardClass HevFshClientForwardClass;

struct _HevFshClientForward
{
    HevFshClientBase base;

    HevTask *task;
    HevFshToken token;
    HevTaskMutex wlock;
};

struct _HevFshClientForwardClass
{
    HevFshClientBaseClass base;
};

HevObjectClass *hev_fsh_client_forward_class (void);

int hev_fsh_client_forward_construct (HevFshClientForward *self,
                                      HevFshConfig *config);

HevFshClientBase *hev_fsh_client_forward_new (HevFshConfig *config);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_FORWARD_H__ */
