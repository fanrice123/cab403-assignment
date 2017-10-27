#ifndef NETWORKS_H
#define NETWORKS_H
#include <stdbool.h>
#include <stdatomic.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include "thrdpool.h"
#define SOC_ADDR_SIZE sizeof(struct sockaddr_in)

extern volatile sig_atomic_t network_suspend;

typedef void *(*validate_func_t)(const int);
typedef void (*void_func_t)(void*);

typedef struct {
    struct sockaddr_in sock_in;
    uint16_t queue_len;
    int sock_fd;
    thrd_pool_t *pool;
} listener_t;

typedef struct connecter {
    struct sockaddr_in sock_in;
    int sock_fd;
} connection_t;

int create_socket(void);

void destroy_socket(int);

listener_t *create_listener(const in_port_t port_no,
                     const int sock_fd,
                     const uint16_t queue_len);

void destroy_listener(listener_t *);

connection_t *create_connection(const in_port_t port_no,
                                const int sock_fd,
                                struct hostent* he);

void destroy_connection(connection_t *);

void listen_to(listener_t *listener, validate_func_t validate, void_func_t execution);


#endif // NETWORKS_H
