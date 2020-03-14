/*
 ============================================================================
 Name        : hev-fsh-base.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2020 everyone.
 Description : Fsh base
 ============================================================================
 */

#include "hev-fsh-base.h"

void
hev_fsh_base_destroy (HevFshBase *self)
{
    if (self->_destroy)
        self->_destroy (self);
}

void
hev_fsh_base_start (HevFshBase *self)
{
    if (self->_start)
        self->_start (self);
}

void
hev_fsh_base_stop (HevFshBase *self)
{
    if (self->_stop)
        self->_stop (self);
}
