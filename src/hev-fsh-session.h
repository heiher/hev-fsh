/*
 ============================================================================
 Name        : hev-fsh-session.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2021 xyz
 Description : Fsh session
 ============================================================================
 */

#ifndef __HEV_FSH_SESSION_H__
#define __HEV_FSH_SESSION_H__

#include <hev-task.h>
#include <hev-task-mutex.h>

#include "hev-rbtree.h"
#include "hev-fsh-io.h"
#include "hev-fsh-protocol.h"
#include "hev-fsh-session-manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_SESSION(p) ((HevFshSession *)p)
#define HEV_FSH_SESSION_CLASS(p) ((HevFshSessionClass *)p)
#define HEV_FSH_SESSION_TYPE (hev_fsh_session_class ())

typedef struct _HevFshSession HevFshSession;
typedef struct _HevFshSessionClass HevFshSessionClass;

struct _HevFshSession
{
    HevFshIO base;

    int client_fd;
    int remote_fd;

    unsigned char type;
    unsigned char is_mgr : 1;
    unsigned char is_temp_token : 1;

    HevFshToken token;
    HevTaskMutex wlock;
    HevRBTreeNode node;

    HevFshSessionManager *manager;
};

struct _HevFshSessionClass
{
    HevFshIOClass base;
};

HevObjectClass *hev_fsh_session_class (void);

int hev_fsh_session_construct (HevFshSession *self, int fd,
                               unsigned int timeout,
                               HevFshSessionManager *manager);

HevFshSession *hev_fsh_session_new (int fd, unsigned int timeout,
                                    HevFshSessionManager *manager);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_SESSION_H__ */
