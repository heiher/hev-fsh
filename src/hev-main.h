/*
 ============================================================================
 Name        : hev-main.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2020 everyone.
 Description : Main
 ============================================================================
 */

#ifndef __HEV_MAIN_H__
#define __HEV_MAIN_H__

#include <netinet/in.h>

#define MAJOR_VERSION (4)
#define MINOR_VERSION (1)
#define MICRO_VERSION (1)

int hev_fsh_parse_sockaddr (struct sockaddr_in6 *saddr, const char *addr,
                            int port);

#endif /* __HEV_MAIN_H__ */
