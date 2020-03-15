/*
 ============================================================================
 Name        : hev-fsh-session.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2020 everyone.
 Description : Fsh session
 ============================================================================
 */

#ifndef __HEV_FSH_SESSION_H__
#define __HEV_FSH_SESSION_H__

#include "hev-task.h"

#define HEV_FSH_SESSION_HP (10)

#define HEV_FSH_SESSION(p) ((HevFshSession *)p)

typedef struct _HevFshSession HevFshSession;
typedef void (*HevFshSessionNotify) (HevFshSession *self, void *data);

struct _HevFshSession
{
    HevFshSession *prev;
    HevFshSession *next;
    HevTask *task;
    int hp;
};

int hev_fsh_session_task_io_yielder (HevTaskYieldType type, void *data);

#endif /* __HEV_FSH_SESSION_H__ */
