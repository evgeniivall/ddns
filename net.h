#ifdef NET_SOCKETS_H
#define NET_SOCKETS_H

typedef int (process_data_cb_t *)(void *data, int length);

int create_socket(int port, int is_server);
void destroy_socket(int sock);

int start_server(int srv_sock, process_data_cb_t process_data);
int connect_to_server(int sock, char *srv_ip, int srv_port);

#endif /* NET_SOCKETS_H */
