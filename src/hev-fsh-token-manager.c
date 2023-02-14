/*
 ============================================================================
 Name        : hev-fsh-token-manager.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2023 xyz
 Description : Fsh token manager
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hev-memory-allocator.h>

#include "hev-logger.h"
#include "hev-compiler.h"

#include "hev-fsh-token-manager.h"

typedef struct _HevFshTokenNode HevFshTokenNode;

struct _HevFshTokenNode
{
    HevRBTreeNode node;
    HevFshToken token;
};

int
hev_fsh_token_manager_find (HevFshTokenManager *self, HevFshToken *token)
{
    HevRBTreeNode **n = &self->tree.root;

    while (*n) {
        HevFshTokenNode *t = container_of (*n, HevFshTokenNode, node);

        int res = memcmp (*token, t->token, sizeof (HevFshToken));
        if (res < 0)
            n = &((*n)->left);
        else if (res > 0)
            n = &((*n)->right);
        else
            return 1;
    }

    return 0;
}

HevFshTokenManager *
hev_fsh_token_manager_new (const char *path)
{
    HevFshTokenManager *self;
    int res;

    self = hev_malloc0 (sizeof (HevFshTokenManager));
    if (!self)
        return NULL;

    res = hev_fsh_token_manager_construct (self, path);
    if (res < 0) {
        hev_free (self);
        return NULL;
    }

    LOG_D ("%p fsh token manager new", self);

    return self;
}

static void
hev_fsh_token_manager_insert (HevFshTokenManager *self, HevFshTokenNode *tn)
{
    HevRBTreeNode **n = &self->tree.root, *parent = NULL;

    while (*n) {
        HevFshTokenNode *t = container_of (*n, HevFshTokenNode, node);
        parent = *n;

        if (memcmp (tn->token, t->token, sizeof (HevFshToken)) < 0)
            n = &((*n)->left);
        else
            n = &((*n)->right);
    }

    hev_rbtree_node_link (&tn->node, parent, n);
    hev_rbtree_insert_color (&self->tree, &tn->node);
}

static void
hev_fsh_token_manager_clear (HevFshTokenManager *self)
{
    HevRBTreeNode *n;

    while ((n = hev_rbtree_first (&self->tree))) {
        HevFshTokenNode *t = container_of (n, HevFshTokenNode, node);
        hev_rbtree_erase (&self->tree, n);
        hev_free (t);
    }
}

void
hev_fsh_token_manager_reload (HevFshTokenManager *self)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t n;

    hev_fsh_token_manager_clear (self);

    fp = fopen (self->path, "r");
    if (!fp) {
        LOG_E ("%p fsh token manager open", self);
        return;
    }

    while ((n = getline (&line, &len, fp)) > 0) {
        HevFshTokenNode *node;
        HevFshToken token;
        int res;

        line[n - 1] = '\0';
        res = hev_fsh_protocol_token_from_string (token, line);
        if (res < 0) {
            LOG_E ("%p fsh token manager parse: %s", self, line);
            continue;
        }

        node = hev_malloc (sizeof (HevFshTokenNode));
        if (!node) {
            LOG_E ("%p fsh token manager alloc", self);
            continue;
        }

        LOG_D ("%p fsh token manager insert: %s", self, line);
        memcpy (node->token, token, sizeof (HevFshToken));
        hev_fsh_token_manager_insert (self, node);
    }

    free (line);
    fclose (fp);
}

int
hev_fsh_token_manager_construct (HevFshTokenManager *self, const char *path)
{
    int res;

    res = hev_object_construct (&self->base);
    if (res < 0)
        return res;

    LOG_D ("%p fsh token manager construct", self);

    self->path = path;
    HEV_OBJECT (self)->klass = HEV_FSH_TOKEN_MANAGER_TYPE;

    return 0;
}

static void
hev_fsh_token_manager_destruct (HevObject *base)
{
    HevFshTokenManager *self = HEV_FSH_TOKEN_MANAGER (base);

    LOG_D ("%p fsh token manager destruct", self);

    HEV_OBJECT_TYPE->finalizer (base);
    hev_free (self);
}

HevObjectClass *
hev_fsh_token_manager_class (void)
{
    static HevFshTokenManagerClass klass;
    HevFshTokenManagerClass *kptr = &klass;
    HevObjectClass *okptr = HEV_OBJECT_CLASS (kptr);

    if (!okptr->name) {
        memcpy (kptr, HEV_OBJECT_TYPE, sizeof (HevObjectClass));

        okptr->name = "HevFshTokenManager";
        okptr->finalizer = hev_fsh_token_manager_destruct;
    }

    return okptr;
}
