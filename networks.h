#ifndef NETWORKS_H
#define NETWORKS_H
#define SOC_ADDR_SIZE sizeof(struct sockaddr_in)

int create_socket(void);

void create_listener(struct sockaddr_in* sock_in,
                     const uint16_t port_no,
                     const sock_fd,
                     const uint16_t queue_len);

void create_connection(struct sockaddr_in* sock_in,
                       const uint16_t port_no,
                       const sock_fd,
                       struct hostent* he);

typedef bool (*bool_func_t)(const int);
typedef void (*void_func_t)(void);

void listen(int socket_fd, bool_func_t validate, void_func_t execution);


#endif // NETWORKS_H
