/*
 ============================================================================
 Name        : hev-fsh-protocol.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 everyone.
 Description : Fsh protocol
 ============================================================================
 */

#ifndef __HEV_FSH_PROTOCOL_H__
#define __HEV_FSH_PROTOCOL_H__

typedef enum _HevFshCommand HevFshCommand;
typedef struct _HevFshMessage HevFshMessage;
typedef struct _HevFshMessageToken HevFshMessageToken;
typedef struct _HevFshMessageTermInfo HevFshMessageTermInfo;
typedef struct _HevFshMessagePortInfo HevFshMessagePortInfo;
typedef unsigned char HevFshToken[16];

enum _HevFshCommand
{
    HEV_FSH_CMD_LOGIN = 0,
    HEV_FSH_CMD_TOKEN,
    HEV_FSH_CMD_KEEP_ALIVE,
    HEV_FSH_CMD_CONNECT,
    HEV_FSH_CMD_ACCEPT,
};

struct _HevFshMessage
{
    unsigned char ver;
    unsigned char cmd;
} __attribute__ ((packed));

struct _HevFshMessageToken
{
    HevFshToken token;
} __attribute__ ((packed));

struct _HevFshMessageTermInfo
{
    unsigned short rows;
    unsigned short columns;
} __attribute__ ((packed));

struct _HevFshMessagePortInfo
{
    unsigned char type;
    unsigned short port;
    unsigned char addr[16];
} __attribute__ ((packed));

void hev_fsh_protocol_token_generate (HevFshToken token);
void hev_fsh_protocol_token_to_string (HevFshToken token, char *out);

int hev_fsh_protocol_token_from_string (HevFshToken token, const char *str);

#endif /* __HEV_FSH_PROTOCOL_H__ */
