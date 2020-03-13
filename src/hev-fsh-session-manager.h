/*
 ============================================================================
 Name        : hev-fsh-session-manager.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2020 everyone.
 Description : Fsh session manager
 ============================================================================
 */

#ifndef __HEV_FSH_SESSION_MANAGER_H__
#define __HEV_FSH_SESSION_MANAGER_H__

#include "hev-fsh-session.h"

typedef struct _HevFshSessionManager HevFshSessionManager;

HevFshSessionManager *hev_fsh_session_manager_new (int timeout, int autostop);
void hev_fsh_session_manager_destroy (HevFshSessionManager *self);

void hev_fsh_session_manager_start (HevFshSessionManager *self);
void hev_fsh_session_manager_stop (HevFshSessionManager *self);

void hev_fsh_session_manager_insert (HevFshSessionManager *self,
                                     HevFshSession *s);
void hev_fsh_session_manager_remove (HevFshSessionManager *self,
                                     HevFshSession *s);

#endif /* __HEV_FSH_SESSION_MANAGER_H__ */
