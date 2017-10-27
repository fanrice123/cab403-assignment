#ifndef GAME_H
#define GAME_H
#include <signal.h>
#define SHRT_BUFF 10
#define LOGIN_FAIL 0
#define LOGIN_SUCCESS 1
#define LOGON 2

extern volatile sig_atomic_t game_suspend;

enum result_t { LOSE, WIN, RESULT_ERR };
    
enum menu_input { PLY = 1, BRD, QUIT };

/// server part

void server_init(void);

void *login(int fd);

int usr_get_fd(void *userp);

void usr_logout(void *userp);

enum menu_input server_interpret(const char *input);

void server_menu(void *userp);

enum result_t server_play(void *userp);

void server_board(void *userp);

void update_board(void *userp, enum result_t);

void server_cleanup(void);

/// client part

void *client_login(int fd);

enum result_t client_play(void *userp);

void client_board(void *userp);

void client_quit(void *userp);

void client_logout(void *userp);

void client_send_result(void *userp, enum result_t result);

#endif // GAME_H
