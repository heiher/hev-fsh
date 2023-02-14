/*
 ============================================================================
 Name        : hev-fsh-server.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2023 xyz
 Description : Fsh server
 ============================================================================
 */

#ifndef __HEV_FSH_SERVER_H__
#define __HEV_FSH_SERVER_H__

#include <hev-task.h>

#include "hev-fsh-base.h"
#include "hev-fsh-config.h"
#include "hev-fsh-token-manager.h"
#include "hev-fsh-session-manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_SERVER(p) ((HevFshServer *)p)
#define HEV_FSH_SERVER_CLASS(p) ((HevFshServerClass *)p)
#define HEV_FSH_SERVER_TYPE (hev_fsh_server_class ())

typedef struct _HevFshServer HevFshServer;
typedef struct _HevFshServerClass HevFshServerClass;

struct _HevFshServer
{
    HevFshBase base;

    int fd;
    int quit;
    int pfds[2];

    HevTask *main_task;
    HevTask *event_task;
    HevFshConfig *config;
    HevFshTokenManager *t_mgr;
    HevFshSessionManager *s_mgr;
};

struct _HevFshServerClass
{
    HevFshBaseClass base;
};

HevObjectClass *hev_fsh_server_class (void);

int hev_fsh_server_construct (HevFshServer *self, HevFshConfig *config);

HevFshBase *hev_fsh_server_new (HevFshConfig *config);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_SERVER_H__ */
