/*
 ============================================================================
 Name        : hev-fsh-token-manager.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2023 xyz
 Description : Fsh token manager
 ============================================================================
 */

#ifndef __HEV_FSH_TOKEN_MANAGER_H__
#define __HEV_FSH_TOKEN_MANAGER_H__

#include "hev-object.h"
#include "hev-rbtree.h"
#include "hev-fsh-protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_FSH_TOKEN_MANAGER(p) ((HevFshTokenManager *)p)
#define HEV_FSH_TOKEN_MANAGER_CLASS(p) ((HevFshTokenManagerClass *)p)
#define HEV_FSH_TOKEN_MANAGER_TYPE (hev_fsh_token_manager_class ())

typedef struct _HevFshTokenManager HevFshTokenManager;
typedef struct _HevFshTokenManagerClass HevFshTokenManagerClass;

struct _HevFshTokenManager
{
    HevObject base;

    HevRBTree tree;
    const char *path;
};

struct _HevFshTokenManagerClass
{
    HevObjectClass base;
};

HevObjectClass *hev_fsh_token_manager_class (void);

int hev_fsh_token_manager_construct (HevFshTokenManager *self,
                                     const char *path);

HevFshTokenManager *hev_fsh_token_manager_new (const char *path);
void hev_fsh_token_manager_reload (HevFshTokenManager *self);

int hev_fsh_token_manager_find (HevFshTokenManager *self, HevFshToken *token);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_FSH_CLIENT_H__ */
