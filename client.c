#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netdb.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include "networks.h"
#include "game.h"

volatile sig_atomic_t suspend = 0;


void main_menu(void *userp);

void print_badge(void);

void sig_handler(int sig);

int main(int argc, char *argv[])
{
    signal(SIGINT, sig_handler);

    if (argc != 3) {
        puts("Server Ip or Port no not found!");
        puts("quit the game...");
        exit(EXIT_FAILURE);
    }

    struct hostent *he = gethostbyname(argv[1]);
    if (!he) {
        herror("get host address failed.");
        exit(EXIT_FAILURE);
    }

    in_port_t port_no;
    sscanf(argv[2], "%"SCNu16, &port_no);
    int sock_fd = create_socket();

    struct connecter *connection = create_connection(port_no, sock_fd, he);
    print_badge();

    bool done = false;
    void *userp = client_login(connection->sock_fd);
    if (userp)
    {
        main_menu(userp);
        // quit game
        client_logout(userp);
        done = true;
    }

    destroy_connection(connection);

    return 0;
}

void main_menu(void *userp)
{
    enum menu_input input;
    do {
    
        //printf("\033[H\033[J");
        int selection;
        printf("\n");
        printf("\n");
        printf("Please enter a selection\n");
        printf("<1> Play Hangman\n");
        printf("<2> Show Leaderboard\n");
        printf("<3> Quit\n");
        printf("\n");
        printf("Selection option 1-3 >");
        do {
            bool got_selection = false;
            char buff[100];
            while (!got_selection) {
                fgets(buff, 100, stdin);
                if (sscanf(buff, "%d\n", &selection) == 1)
                    got_selection = true;
                else
                    puts("Unknown selection. Please input 1 to 3");
            }
            fflush(stdin);
            if (suspend) {
                client_quit(userp);
                return;
            } else if (1 <= selection && selection <= 3) {
                break;
            } else {
                puts("Please enter value between 1 and 3");
            }
        } while(1);

        input = (enum menu_input) selection;
    
        switch (input) {
        case PLY:
            printf("\033[H\033[J");
            enum result_t result = client_play(userp);
            client_send_result(userp, result);
            break;
        case BRD:
            client_board(userp);
            break;
        case QUIT:
            client_quit(userp);
            break;
        }
    } while (input != QUIT && !suspend);
};

void print_badge(void) {
    printf("=====================================================================\n");
    printf("\n");
    printf("Welcome to the Online Hangman Gaming System!\n");
    printf("\n");
    printf("=====================================================================\n");
}

void sig_handler(int sig) {
    suspend = 1;
}
