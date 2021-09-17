/*
 ============================================================================
 Name        : hev-fsh-session-manager.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2020 - 2021 xyz
 Description : Fsh session manager
 ============================================================================
 */

#ifndef __HEV_FSH_SESSION_MANAGER_H__
#define __HEV_FSH_SESSION_MANAGER_H__

#include "hev-object.h"
#include "hev-rbtree.h"
#include "hev-fsh-protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_SESSION_MANAGER(p) ((HevFshSessionManager *)p)
#define HEV_FSH_SESSION_MANAGER_CLASS(p) ((HevFshSessionManagerClass *)p)
#define HEV_FSH_SESSION_MANAGER_TYPE (hev_fsh_session_manager_class ())

typedef struct _HevFshSession HevFshSession;
typedef struct _HevFshSessionManager HevFshSessionManager;
typedef struct _HevFshSessionManagerClass HevFshSessionManagerClass;

struct _HevFshSessionManager
{
    HevObject base;

    HevRBTree tree;
};

struct _HevFshSessionManagerClass
{
    HevObjectClass base;
};

HevObjectClass *hev_fsh_session_manager_class (void);

int hev_fsh_session_manager_construct (HevFshSessionManager *self);

HevFshSessionManager *hev_fsh_session_manager_new (void);

void hev_fsh_session_manager_insert (HevFshSessionManager *self,
                                     HevFshSession *s);

void hev_fsh_session_manager_remove (HevFshSessionManager *self,
                                     HevFshSession *s);

HevFshSession *hev_fsh_session_manager_find (HevFshSessionManager *self,
                                             int type, HevFshToken *token);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_H__ */
