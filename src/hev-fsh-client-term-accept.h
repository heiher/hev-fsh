/*
 ============================================================================
 Name        : hev-fsh-client-term-accept.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2021 xyz
 Description : Fsh client term accept
 ============================================================================
 */

#ifndef __HEV_FSH_CLIENT_TERM_ACCEPT_H__
#define __HEV_FSH_CLIENT_TERM_ACCEPT_H__

#include "hev-fsh-client-accept.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_CLIENT_TERM_ACCEPT(p) ((HevFshClientTermAccept *)p)
#define HEV_FSH_CLIENT_TERM_ACCEPT_CLASS(P) ((HevFshClientTermAcceptClass *)p)
#define HEV_FSH_CLIENT_TERM_ACCEPT_TYPE (hev_fsh_client_term_accept_class ())

typedef struct _HevFshClientTermAccept HevFshClientTermAccept;
typedef struct _HevFshClientTermAcceptClass HevFshClientTermAcceptClass;

struct _HevFshClientTermAccept
{
    HevFshClientAccept base;
};

struct _HevFshClientTermAcceptClass
{
    HevFshClientAcceptClass base;
};

HevObjectClass *hev_fsh_client_term_accept_class (void);

int hev_fsh_client_term_accept_construct (HevFshClientTermAccept *self,
                                          HevFshConfig *config,
                                          HevFshToken token);

HevFshClientBase *hev_fsh_client_term_accept_new (HevFshConfig *config,
                                                  HevFshToken token);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_TERM_ACCEPT_H__ */
