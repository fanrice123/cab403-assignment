#include "networks.h"
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#define AUTO_PROTOCOL 0
#define exit_if(FLAG,MSG) if (FLAG) { \
                              perror(MSG); \
                              exit(EXIT_FAILURE); \
                          }
struct listener {
    struct sockaddr_in sock_in;
    uint16_t queue_len;
    int sock_fd;
};


static void init_sockaddr(struct sockaddr_in* sock_in, const uint16_t port_no) {
    sock_in->sin_family = AF_INET;
    sock_in->sin_port = htons(port_no);
}

int create_socket(void) {

    int sock_fd = socket(AF_INET, SOCK_STREAM, AUTO_PROTOCOL);

    exit_if(sock_fd == -1, "socket request failed.");

    return sock_fd;
}

struct listener *create_listener(const uint16_t port_no, 
                     const int sock_fd, 
                     const uint16_t queue_len) {
    struct listener *new_l = (void*) malloc(sizeof(struct listener));
    struct sockaddr_in *sock_in = &new_l->sock_in;

    init_sockaddr(sock_in, port_no);
    sock_in->sin_addr.s_addr = INADDR_ANY;
    new_l->queue_len = queue_len;
    new_l->sock_fd = sock_fd;


    exit_if(!bind(sock_fd, (struct sockaddr*)sock_in, sizeof(struct sockaddr)), "socket binding failed.");

    return new_l;

    //exit_if(!listen(sock_fd, queue_len), "listen failed.");
}
    
void create_connection(struct sockaddr_in* sock_in,
                       const uint16_t port_no,
                       const sock_fd,
                       struct hostent* he) {
    init_sockaddr(sock_in, port_no);
    sock_in->sin_addr = *((struct in_addr*) he->h_addr_list[0]);
    
    exit_if(!connect(sock_fd, (struct sockaddr*)sock_in, sizeof(struct sockaddr)), "connection establishment failed.");
}

void listen(int socket_fd, bool_func validate, void_func execution) {
    int socket_fd = listnr.
    struct sockaddr_in in_coming;
    int private_fd;

    while(true) {
        exit_if(!(private_fd = accept(sockfd, (struct sockaddr*)&in_coming, SOC_ADDR_SIZE)), "accept connection failed");
        printf("connection from %s\n", inet_ntoa(in_coming.sin_addr));

        fputs("waiting for data....", stdout);
        void* userp = validate(private_fd);

        if (userp) {
            execution(userp);
        } else {
            fputs(stderr, "validation failed, operation aborted.\n");
            // TODO
            // clear current working thread

    }
        
        

