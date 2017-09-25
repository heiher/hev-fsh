/*
 ============================================================================
 Name        : hev-fsh-server-session.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 everyone.
 Description : Fsh server session
 ============================================================================
 */

#ifndef __HEV_FSH_SERVER_SESSION_H__
#define __HEV_FSH_SERVER_SESSION_H__

#include "hev-task.h"

typedef struct _HevFshServerSessionBase HevFshServerSessionBase;
typedef struct _HevFshServerSession HevFshServerSession;
typedef void (*HevFshServerSessionCloseNotify) (HevFshServerSession *self, void *data);

struct _HevFshServerSessionBase
{
	HevFshServerSessionBase *prev;
	HevFshServerSessionBase *next;
	HevTask *task;
	int hp;
};

HevFshServerSession * hev_fsh_server_session_new (int client_fd,
			HevFshServerSessionCloseNotify notify, void *notify_data);

HevFshServerSession * hev_fsh_server_session_ref (HevFshServerSession *self);
void hev_fsh_server_session_unref (HevFshServerSession *self);

void hev_fsh_server_session_run (HevFshServerSession *self);

#endif /* __HEV_FSH_SERVER_SESSION_H__ */

