/*
 ============================================================================
 Name        : hev-fsh-session.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2020 everyone.
 Description : Fsh session
 ============================================================================
 */

#include "hev-fsh-session.h"

int
hev_fsh_session_task_io_yielder (HevTaskYieldType type, void *data)
{
    HevFshSession *s = data;

    s->hp = HEV_FSH_SESSION_HP;
    hev_task_yield (type);

    return (s->hp > 0) ? 0 : -1;
}
