/*
 ============================================================================
 Name        : hev-fsh-config.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2018 everyone.
 Description : Fsh Config
 ============================================================================
 */

#ifndef __HEV_FSH_CONFIG_H__
#define __HEV_FSH_CONFIG_H__

typedef struct _HevFshConfig HevFshConfig;
typedef enum _HevFshConfigMode HevFshConfigMode;

enum _HevFshConfigMode
{
    HEV_FSH_CONFIG_MODE_NONE = 0,
    HEV_FSH_CONFIG_MODE_SERVER = (1 << 3),
    HEV_FSH_CONFIG_MODE_FORWARDER = (1 << 2),
    HEV_FSH_CONFIG_MODE_FORWARDER_TERM = (1 << 2),
    HEV_FSH_CONFIG_MODE_FORWARDER_PORT = (1 << 2) | 1,
    HEV_FSH_CONFIG_MODE_CONNECTOR = (1 << 1),
    HEV_FSH_CONFIG_MODE_CONNECTOR_TERM = (1 << 1),
    HEV_FSH_CONFIG_MODE_CONNECTOR_PORT = (1 << 1) | 1,
};

HevFshConfig *hev_fsh_config_new (void);
void hev_fsh_config_destroy (HevFshConfig *self);

/* Common */
int hev_fsh_config_get_mode (HevFshConfig *self);
void hev_fsh_config_set_mode (HevFshConfig *self, int val);

const char *hev_fsh_config_get_server_address (HevFshConfig *self);
void hev_fsh_config_set_server_address (HevFshConfig *self, const char *val);

unsigned int hev_fsh_config_get_server_port (HevFshConfig *self);
void hev_fsh_config_set_server_port (HevFshConfig *self, unsigned int val);

const char *hev_fsh_config_get_token (HevFshConfig *self);
void hev_fsh_config_set_token (HevFshConfig *self, const char *val);

const char *hev_fsh_config_get_log (HevFshConfig *self);
void hev_fsh_config_set_log (HevFshConfig *self, const char *val);

/* Forwarder terminal */
const char *hev_fsh_config_get_user (HevFshConfig *self);
void hev_fsh_config_set_user (HevFshConfig *self, const char *val);

/* Forwarder port */
int hev_fsh_config_addr_list_contains (HevFshConfig *self, int type, void *addr,
                                       unsigned int port);
void hev_fsh_config_addr_list_add (HevFshConfig *self, int type, void *addr,
                                   unsigned int port, int action);

/* Connector port */
const char *hev_fsh_config_get_local_address (HevFshConfig *self);
void hev_fsh_config_set_local_address (HevFshConfig *self, const char *val);

unsigned int hev_fsh_config_get_local_port (HevFshConfig *self);
void hev_fsh_config_set_local_port (HevFshConfig *self, unsigned int val);

const char *hev_fsh_config_get_remote_address (HevFshConfig *self);
void hev_fsh_config_set_remote_address (HevFshConfig *self, const char *val);

unsigned int hev_fsh_config_get_remote_port (HevFshConfig *self);
void hev_fsh_config_set_remote_port (HevFshConfig *self, unsigned int val);

#endif /* __HEV_CONFIG_H__ */
