#include "networks.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <netinet/in.h>
#include "list.h"
#include "thrdpool.h"

#define AUTO_PROTOCOL 0
#define exit_if(FLAG,MSG) do { \
                              if (FLAG) { \
                                  perror(MSG); \
                                  exit(EXIT_FAILURE); \
                              } \
                          } while (0)

volatile sig_atomic_t network_suspend = 0;

struct task_patch {
    int sock_fd;
    validate_func_t validate;
    void_func_t execution;
};

static atomic_int private_fds[10];

static void split_listen(void *);

static void init_sockaddr(struct sockaddr_in*, const in_port_t);

int create_socket(void) {

    int sock_fd = socket(AF_INET, SOCK_STREAM, AUTO_PROTOCOL);

    exit_if(sock_fd == -1, "socket request failed.");

    return sock_fd;
}

void destroy_socket(int sock_fd) {
    close(sock_fd);
}

listener_t *create_listener(const in_port_t port_no,
                     const int sock_fd,
                     const uint16_t queue_len) {
    listener_t *new_l = (void*) malloc(sizeof(listener_t));
    struct sockaddr_in *sock_in = &new_l->sock_in;

    init_sockaddr(sock_in, port_no);
    sock_in->sin_addr.s_addr = INADDR_ANY;
    //bzero(&sock_in->sin_zero), 8);
    new_l->queue_len = queue_len;
    new_l->sock_fd = sock_fd;

    exit_if(bind(sock_fd, (struct sockaddr*)sock_in, sizeof(struct sockaddr)) != 0, "socket binding failed.");
    exit_if(listen(sock_fd, queue_len), "listen failed.");

    new_l->pool = pool_create(10);

    return new_l;
}

void destroy_listener(listener_t *listener) {
    pool_destroy(listener->pool);
    free(listener);
}

connection_t *create_connection(const in_port_t port_no,
                               const int sock_fd,
                               struct hostent* he) {
    connection_t *c = malloc(sizeof(connection_t));
    init_sockaddr(&c->sock_in, port_no);
    c->sock_in.sin_addr = *((struct in_addr*) he->h_addr_list[0]);
    c->sock_fd = sock_fd;
    bzero(&c->sock_in.sin_zero, 8); 

    exit_if(connect(sock_fd, (struct sockaddr*)&c->sock_in, sizeof(struct sockaddr)), "connection establishment failed.");

    return c;
}

void destroy_connection(connection_t *connection) {
    free(connection);
}

void listen_to(listener_t *listener, validate_func_t validate, void_func_t execution) {
    int sock_fd = listener->sock_fd;
    struct sockaddr_in in_coming;
    socklen_t sock_size = SOC_ADDR_SIZE;
    for (int i = i; i != 10; ++i)
        private_fds[i] = -1;

    while(!network_suspend) {
        int private_fd = accept(sock_fd, (struct sockaddr*)&in_coming, &sock_size);
        for (int i = 0; i != 10; ++i) {
            if (private_fds[i] != -1) {
                private_fds[i] = private_fd;
                break;
            }
        }
        
        if (network_suspend) {
            fprintf(stderr, "\nCTRL+C pressed. Aborting...\n");
            break;
        }
        exit_if(private_fd == -1, "accept connection failed");
        fprintf(stderr, "connection from %s\n", inet_ntoa(in_coming.sin_addr));

        fputs("waiting for data....\n", stderr);
        struct task_patch patch = { private_fd, validate, execution };
        add_task(listener->pool, split_listen, &patch, sizeof(struct task_patch));


    }
    fputs("Cleaning socket...", stderr);

    for (int i = 0; i != 10; ++i) {
        if (private_fds[i] != -1)
            close(private_fds[i]);
    }

}

static void split_listen(void *void_patch) {
    void *userp;
    struct task_patch *patch = void_patch;
    int sock_fd = patch->sock_fd;

    if (userp = patch->validate(sock_fd)) {
        patch->execution(userp);
    } else {
        fputs("validation failed, operation aborted.\n", stderr);
    }

    for (int i = 0; i != 10; ++i) {
        if (private_fds[i] == sock_fd) {
            private_fds[i] = -1;
        }
    }
}

static void init_sockaddr(struct sockaddr_in* sock_in, const in_port_t port_no) {
    sock_in->sin_family = AF_INET;
    sock_in->sin_port = htons(port_no);
}

