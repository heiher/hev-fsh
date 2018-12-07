/*
 ============================================================================
 Name        : hev-fsh-protocol.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 everyone.
 Description : Fsh protocol
 ============================================================================
 */

#if defined(__APPLE__) || (__MACH__)
#include <uuid/uuid.h>
#else
#include <uuid.h>
#endif

#include "hev-fsh-protocol.h"

void
hev_fsh_protocol_token_generate (HevFshToken token)
{
    uuid_generate_random (token);
}

void
hev_fsh_protocol_token_to_string (HevFshToken token, char *out)
{
    uuid_unparse (token, out);
}

int
hev_fsh_protocol_token_from_string (HevFshToken token, const char *str)
{
    return uuid_parse (str, token);
}
