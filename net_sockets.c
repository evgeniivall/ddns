#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "net_sockets.h"

static int str2sockaddr_in(struct sockaddr_in *dst, char *ip, int port)
{
    memset(dst, 0, sizeof(struct sockaddr_in));
    dst->sin_family = AF_INET;

    if ((inet_pton(AF_INET, ip, &dst->sin_addr) != 1))
    {
        fprintf(stderr, "Bad IP address\n");
        return -1;
    }
    dst->sin_port = htons(port);

    return 0;
}

int create_socket(int port, int timeout)
{
    int sock;
    struct sockaddr_in address;
    struct timeval tv;

    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
	goto Error;

    if (timeout)
    {
        tv.tv_sec = 0;
        tv.tv_usec = timeout;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
            goto Error;
    }
    if (bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0)
	goto Error;

    return sock;

Error:
    fprintf(stderr, "%s\n", strerror(errno));
    return -1;

}

void destroy_socket(int sock)
{
    close(sock);
}

int recv_data(int sock, void *buff, int buff_size, char *ip)
{
    int rv;
    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(struct sockaddr_in);

    rv = recvfrom(sock, buff, buff_size, 0, (struct sockaddr *)&client_address,
		                   &client_address_len);
    if (rv > 0 && ip)
        strcpy(ip, inet_ntoa(client_address.sin_addr));

    return rv;
}

int send_data(int sock, void *data, int data_size, char *ip, int port)
{
    struct sockaddr_in addr;

    if (str2sockaddr_in(&addr, ip, port) == -1)
        return -1;
    return sendto(sock, data, data_size, 0,
            (struct sockaddr *)&addr, sizeof(addr));
}
