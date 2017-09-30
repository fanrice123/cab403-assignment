#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include "networks.h"

#define DEFAULT_PORT 12345u
#define QUEUE_LEN 100
#define AUTO_PROTOCOL 0

int main(int argc, char* argv[]) {
    struct sockaddr_in listener;
    uint16_t port_no; 

    // set the port numbr first
    if (argc == 2)
	    port_no = argv[1];
    else
	    port_no = DEFAULT_PORT;

    int sock_fd = create_socket(&sock_fd);

    create_listener(&listener, port_no, sock_fd, QUEUE_LEN);

    printf("Starts listening to port %hu...\n", port_no);

    listen(sock_fd, login, controller);

    return 0;
}




