#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include "networks.h"
#include "game.h"
#define _POSIX_C_SOURCE 200809L

#define DEFAULT_PORT 12345
#define QUEUE_LEN 100
#define AUTO_PROTOCOL 0

void handle_signal(int sig);

int main(int argc, char* argv[]) {
    struct sigaction sa;
    in_port_t port_no;

    sa.sa_handler = &handle_signal;
    sa.sa_flags = 0;
    //sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGINT");
        exit(EXIT_FAILURE);
    }

    // set the port numbr first
    if (argc == 2)
	    sscanf(argv[1], "%"SCNu16, &port_no);
    else
	    port_no = DEFAULT_PORT;

    int sock_fd = create_socket();

    listener_t * listener = create_listener(port_no, sock_fd, QUEUE_LEN);

    printf("Starts listening to port %hu...\n", port_no);

    server_init();

    listen_to(listener, login, server_menu);

    destroy_listener(listener);

    destroy_socket(sock_fd);

    return 0;
}

void handle_signal(int sig) {
    thrd_pool_suspend = 1;
    game_suspend = 1;
    network_suspend = 1;
    // kill(getpid(), SIGUSR1);
}
