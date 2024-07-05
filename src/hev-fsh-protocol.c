/*
 ============================================================================
 Name        : hev-fsh-protocol.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2021 xyz
 Description : Fsh protocol
 ============================================================================
 */

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hev-random.h"

#include "hev-fsh-protocol.h"

#define HEV_FSH_UUID_FMT "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"

static void
pack (HevFshToken token, uint32_t time_low, uint16_t time_mid,
      uint16_t time_high_ver, uint16_t clock_seq)
{
    token[3] = (unsigned char)time_low;
    time_low >>= 8;
    token[2] = (unsigned char)time_low;
    time_low >>= 8;
    token[1] = (unsigned char)time_low;
    time_low >>= 8;
    token[0] = (unsigned char)time_low;

    token[5] = (unsigned char)time_mid;
    time_mid >>= 8;
    token[4] = (unsigned char)time_mid;

    token[7] = (unsigned char)time_high_ver;
    time_high_ver >>= 8;
    token[6] = (unsigned char)time_high_ver;

    token[9] = (unsigned char)clock_seq;
    clock_seq >>= 8;
    token[8] = (unsigned char)clock_seq;
}

static void
unpack (HevFshToken token, uint32_t *time_low, uint16_t *time_mid,
        uint16_t *time_high_ver, uint16_t *clock_seq)
{
    const uint8_t *ptr = token;

    *time_low = *ptr++;
    *time_low = (*time_low << 8) | *ptr++;
    *time_low = (*time_low << 8) | *ptr++;
    *time_low = (*time_low << 8) | *ptr++;

    *time_mid = *ptr++;
    *time_mid = (*time_mid << 8) | *ptr++;

    *time_high_ver = *ptr++;
    *time_high_ver = (*time_high_ver << 8) | *ptr++;

    *clock_seq = *ptr++;
    *clock_seq = (*clock_seq << 8) | *ptr++;
}

void
hev_fsh_protocol_token_generate (HevFshToken token)
{
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_high_ver;
    uint16_t clock_seq;

    hev_random_get_bytes (token, sizeof (HevFshToken));

    unpack (token, &time_low, &time_mid, &time_high_ver, &clock_seq);
    clock_seq = (clock_seq & 0x3FFF) | 0x8000;
    time_high_ver = (time_high_ver & 0x0FFF) | 0x4000;
    pack (token, time_low, time_mid, time_high_ver, clock_seq);
}

void
hev_fsh_protocol_token_to_string (HevFshToken token, char *out)
{
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_high_ver;
    uint16_t clock_seq;

    unpack (token, &time_low, &time_mid, &time_high_ver, &clock_seq);
    sprintf (out, HEV_FSH_UUID_FMT, time_low, time_mid, time_high_ver,
             clock_seq >> 8, clock_seq & 0xFF, token[10], token[11], token[12],
             token[13], token[14], token[15]);
}

int
hev_fsh_protocol_token_from_string (HevFshToken token, const char *str)
{
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_high_ver;
    uint16_t clock_seq;
    const char *cp;
    char buf[3];
    int i;

    if (strlen (str) != HEV_FSH_TOKEN_STR_LEN)
        return -1;

    for (i = 0, cp = str; i <= HEV_FSH_TOKEN_STR_LEN; i++, cp++) {
        if ((i == 8) || (i == 13) || (i == 18) || (i == 23)) {
            if (*cp == '-')
                continue;
            else
                return -1;
        }
        if (i == HEV_FSH_TOKEN_STR_LEN) {
            if (*cp == 0)
                continue;
        }
        if (!isxdigit (*cp))
            return -1;
    }

    time_low = strtoul (str, NULL, 16);
    time_mid = strtoul (str + 9, NULL, 16);
    time_high_ver = strtoul (str + 14, NULL, 16);
    clock_seq = strtoul (str + 19, NULL, 16);
    pack (token, time_low, time_mid, time_high_ver, clock_seq);

    buf[2] = 0;
    cp = str + 24;
    for (i = 0; i < 6; i++) {
        buf[0] = *cp++;
        buf[1] = *cp++;
        token[10 + i] = strtoul (buf, NULL, 16);
    }

    return 0;
}
