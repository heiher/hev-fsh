/*
 ============================================================================
 Name        : hev-fsh-session-manager.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2021 xyz
 Description : Fsh session manager
 ============================================================================
 */

#include <string.h>

#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-compiler.h"
#include "hev-fsh-session.h"

#include "hev-fsh-session-manager.h"

void
hev_fsh_session_manager_insert (HevFshSessionManager *self, HevFshSession *s)
{
    HevRBTreeNode **n = &self->tree.root, *parent = NULL;

    while (*n) {
        HevFshSession *t = container_of (*n, HevFshSession, node);

        parent = *n;

        if (s->type < t->type) {
            n = &((*n)->left);
        } else if (s->type > t->type) {
            n = &((*n)->right);
        } else {
            if (memcmp (s->token, t->token, sizeof (HevFshToken)) < 0)
                n = &((*n)->left);
            else
                n = &((*n)->right);
        }
    }

    hev_rbtree_node_link (&s->node, parent, n);
    hev_rbtree_insert_color (&self->tree, &s->node);
}

void
hev_fsh_session_manager_remove (HevFshSessionManager *self, HevFshSession *s)
{
    hev_rbtree_erase (&self->tree, &s->node);
}

HevFshSession *
hev_fsh_session_manager_find (HevFshSessionManager *self, int type,
                              HevFshToken *token)
{
    HevRBTreeNode **n = &self->tree.root;

    while (*n) {
        HevFshSession *t = container_of (*n, HevFshSession, node);

        if (type < t->type) {
            n = &((*n)->left);
        } else if (type > t->type) {
            n = &((*n)->right);
        } else {
            int res = memcmp (*token, t->token, sizeof (HevFshToken));
            if (res < 0)
                n = &((*n)->left);
            else if (res > 0)
                n = &((*n)->right);
            else
                return t;
        }
    }

    return NULL;
}

HevFshSessionManager *
hev_fsh_session_manager_new (void)
{
    HevFshSessionManager *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshSessionManager));
    if (!self)
        return NULL;

    res = hev_fsh_session_manager_construct (self);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh session manager new", self);

    return self;
}

int
hev_fsh_session_manager_construct (HevFshSessionManager *self)
{
    int res;

    res = hev_object_construct (&self->base);
    if (res < 0)
        return res;

    LOG_D ("%p fsh session manager construct", self);

    HEV_OBJECT (self)->klass = HEV_FSH_SESSION_MANAGER_TYPE;

    return 0;
}

static void
hev_fsh_session_manager_destruct (HevObject *base)
{
    HevFshSessionManager *self = HEV_FSH_SESSION_MANAGER (base);

    LOG_D ("%p fsh session manager destruct", self);

    HEV_OBJECT_TYPE->destruct (base);
    hev_free (self);
}

HevObjectClass *
hev_fsh_session_manager_class (void)
{
    static HevFshSessionManagerClass klass;
    HevFshSessionManagerClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        memcpy (kptr, HEV_OBJECT_TYPE, sizeof (HevObjectClass));

        okptr->name = "HevFshSessionManager";
        okptr->destruct = hev_fsh_session_manager_destruct;
    }

    return okptr;
}
