/*
 ============================================================================
 Name        : hev-socks5-server-us.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2021 hev
 Description : Socks5 Server Userspace
 ============================================================================
 */

#ifndef __HEV_SOCKS5_SERVER_US_H__
#define __HEV_SOCKS5_SERVER_US_H__

#include "hev-socks5-server.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEV_SOCKS5_SERVER_US(p) ((HevSocks5ServerUS *)p)
#define HEV_SOCKS5_SERVER_US_CLASS(p) ((HevSocks5ServerUSClass *)p)
#define HEV_SOCKS5_SERVER_US_TYPE (hev_socks5_server_us_class ())

typedef struct _HevSocks5ServerUS HevSocks5ServerUS;
typedef struct _HevSocks5ServerUSClass HevSocks5ServerUSClass;

struct _HevSocks5ServerUS
{
    HevSocks5Server base;
};

struct _HevSocks5ServerUSClass
{
    HevSocks5ServerClass base;
};

HevObjectClass *hev_socks5_server_us_class (void);

int hev_socks5_server_us_construct (HevSocks5ServerUS *self, int fd);

HevSocks5Server *hev_socks5_server_us_new (int fd);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_SOCKS5_SERVER_US_H__ */
