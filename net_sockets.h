#ifndef NET_SOCKETS_H
#define NET_SOCKETS_H
#include <arpa/inet.h>

void destroy_socket(int sock);
int create_socket(int port, int timeout);

int recv_data(int sock, void *buff, int buff_size, char *ip);
int send_data(int sock, void *data, int data_size, char *ip, int port);

#endif /* NET_SOCKETS_H */
