#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#define QUEUE_SIZE 16
#define BUFF_SIZE 64

int create_socket(int port, int is_server)
{
    int sock;
    struct sockaddr_in address;

    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	goto Error;

    if ((bind(sock, (struct sockaddr *)&address, sizeof(address))) < 0)
	goto Error;

    if (!is_server)
	return sock;

    if ((listen(sock, QUEUE_SIZE) < 0))
	goto Error;

Error:
    fprintf(stderr, "%s\n", strerror(errno));
    return -1;

}

void destroy_socket(int sock)
{
    close(sock);
}

int connect_to_server(int sock, char *srv_ip, int srv_port)
{
    struct sockaddr_in srv_addr;

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(srv_port);
    inet_pton(AF_INET, srv_ip, &srv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
    {
	fprintf(stderr, "%s\n", strerror(errno));
	return -1;
    }

    return 0;
}


int start_server(int srv_sock, process_data_cb_t process_data)
{
    char buff[BUFF_SIZE];
    int length, peer_sock;

    struct sockaddr_in peer_addr;
    socklen_t addr_size;

    while (1)
    {
	peer_sock = accept(srv_sock, (struct sockaddr*) &peer_addr, &addr_size);
	length = recv(peer_sock, buff, BUFF_SIZE, 0);
	/* TODO Add error check */

	process_data(buff, length);
    }
}
