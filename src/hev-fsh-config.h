/*
 ============================================================================
 Name        : hev-fsh-config.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2018 - 2023 xyz
 Description : Fsh Config
 ============================================================================
 */

#ifndef __HEV_FSH_CONFIG_H__
#define __HEV_FSH_CONFIG_H__

#include <netinet/in.h>

#define HEV_FSH_CONFIG_TASK_STACK_SIZE (16384)

typedef struct _HevFshConfig HevFshConfig;
typedef struct _HevFshConfigKey HevFshConfigKey;
typedef enum _HevFshConfigMode HevFshConfigMode;

enum _HevFshConfigMode
{
    HEV_FSH_CONFIG_MODE_NONE = 0,
    HEV_FSH_CONFIG_MODE_SERVER = (1 << 4),
    HEV_FSH_CONFIG_MODE_FORWARDER = (1 << 3),
    HEV_FSH_CONFIG_MODE_FORWARDER_TERM = (1 << 3),
    HEV_FSH_CONFIG_MODE_FORWARDER_PORT = (1 << 3) | (1 << 0),
    HEV_FSH_CONFIG_MODE_FORWARDER_SOCK = (1 << 3) | (1 << 1),
    HEV_FSH_CONFIG_MODE_CONNECTOR = (1 << 2),
    HEV_FSH_CONFIG_MODE_CONNECTOR_TERM = (1 << 2),
    HEV_FSH_CONFIG_MODE_CONNECTOR_PORT = (1 << 2) | (1 << 0),
    HEV_FSH_CONFIG_MODE_CONNECTOR_SOCK = (1 << 2) | (1 << 1),
};

struct _HevFshConfigKey
{
    unsigned char key[16];
    unsigned char salt[4];
};

HevFshConfig *hev_fsh_config_new (void);
void hev_fsh_config_destroy (HevFshConfig *self);

/* Common */
int hev_fsh_config_get_mode (HevFshConfig *self);
void hev_fsh_config_set_mode (HevFshConfig *self, int val);

const char *hev_fsh_config_get_server_address (HevFshConfig *self);
void hev_fsh_config_set_server_address (HevFshConfig *self, const char *val);

const char *hev_fsh_config_get_server_port (HevFshConfig *self);
void hev_fsh_config_set_server_port (HevFshConfig *self, const char *val);

const char *hev_fsh_config_get_tokens_file (HevFshConfig *self);
void hev_fsh_config_set_tokens_file (HevFshConfig *self, const char *val);

const char *hev_fsh_config_get_token (HevFshConfig *self);
void hev_fsh_config_set_token (HevFshConfig *self, const char *val);

const char *hev_fsh_config_get_log_path (HevFshConfig *self);
void hev_fsh_config_set_log_path (HevFshConfig *self, const char *val);

int hev_fsh_config_get_log_level (HevFshConfig *self);
void hev_fsh_config_set_log_level (HevFshConfig *self, int val);

int hev_fsh_config_get_ip_type (HevFshConfig *self);
void hev_fsh_config_set_ip_type (HevFshConfig *self, int val);

unsigned int hev_fsh_config_get_timeout (HevFshConfig *self);
void hev_fsh_config_set_timeout (HevFshConfig *self, unsigned int val);

HevFshConfigKey *hev_fsh_config_get_key (HevFshConfig *self);
void hev_fsh_config_set_key (HevFshConfig *self, HevFshConfigKey *val,
                             int ugly_ktls);

const char *hev_fsh_config_get_tcp_cc (HevFshConfig *self);
void hev_fsh_config_set_tcp_cc (HevFshConfig *self, const char *val);

/* Forwarder terminal */
const char *hev_fsh_config_get_user (HevFshConfig *self);
void hev_fsh_config_set_user (HevFshConfig *self, const char *val);

/* Forwarder port */
int hev_fsh_config_addr_list_contains (HevFshConfig *self, int type, void *addr,
                                       unsigned int port);
void hev_fsh_config_addr_list_add (HevFshConfig *self, int type, void *addr,
                                   unsigned int port, int action);

/* Connector port | sock */
const char *hev_fsh_config_get_local_address (HevFshConfig *self);
void hev_fsh_config_set_local_address (HevFshConfig *self, const char *val);

unsigned int hev_fsh_config_get_local_port (HevFshConfig *self);
void hev_fsh_config_set_local_port (HevFshConfig *self, unsigned int val);

const char *hev_fsh_config_get_remote_address (HevFshConfig *self);
void hev_fsh_config_set_remote_address (HevFshConfig *self, const char *val);

unsigned int hev_fsh_config_get_remote_port (HevFshConfig *self);
void hev_fsh_config_set_remote_port (HevFshConfig *self, unsigned int val);

/* Helper */
struct sockaddr *hev_fsh_config_get_server_sockaddr (HevFshConfig *self,
                                                     socklen_t *len);
struct sockaddr *hev_fsh_config_get_local_sockaddr (HevFshConfig *self,
                                                    socklen_t *len);

int hev_fsh_config_is_ugly_ktls (HevFshConfig *self);

#endif /* __HEV_CONFIG_H__ */
