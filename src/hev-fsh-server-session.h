/*
 ============================================================================
 Name        : hev-fsh-server-session.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2020 everyone.
 Description : Fsh server session
 ============================================================================
 */

#ifndef __HEV_FSH_SERVER_SESSION_H__
#define __HEV_FSH_SERVER_SESSION_H__

#include "hev-rbtree.h"
#include "hev-fsh-session.h"

#define HEV_FSH_SERVER_SESSION(p) ((HevFshServerSession *)p)

typedef struct _HevFshServerSession HevFshServerSession;

HevFshServerSession *hev_fsh_server_session_new (int client_fd,
                                                 HevFshSessionNotify notify,
                                                 void *notify_data,
                                                 HevRBTree *sessions_tree);

void hev_fsh_server_session_run (HevFshServerSession *self);

#endif /* __HEV_FSH_SERVER_SESSION_H__ */
