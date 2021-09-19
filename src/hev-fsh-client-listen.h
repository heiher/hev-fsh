/*
 ============================================================================
 Name        : hev-fsh-client-listen.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh client listen
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_LISTEN_H__
#define __HEV_FSH_CLIENT_LISTEN_H__

#include "hev-fsh-config.h"
#include "hev-fsh-client-base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_LISTEN(p) ((HevFshClientListen *)p)
#define HEV_FSH_CLIENT_LISTEN_CLASS(p) ((HevFshClientListenClass *)p)
#define HEV_FSH_CLIENT_LISTEN_TYPE (hev_fsh_client_listen_class ())

typedef struct _HevFshClientListen HevFshClientListen;
typedef struct _HevFshClientListenClass HevFshClientListenClass;

struct _HevFshClientListen
{
    HevFshClientBase base;
};

struct _HevFshClientListenClass
{
    HevFshClientBaseClass base;

    void (*dispatch) (HevFshClientListen *self, int fd);
};

HevObjectClass *hev_fsh_client_listen_class (void);

int hev_fsh_client_listen_construct (HevFshClientListen *self,
                                     HevFshConfig *config);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_LISTEN_H__ */
