#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdatomic.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include "kread_list.h"
#include "list.h"
#define INPUT_LEN 128
#define BUFF_LEN 256
#define DO_STR(X) #X
#define STR(X) DO_STR(X)
#define SERVER_BRD_FRMT "id:%s,won:%d,played:%d"
#define CLIENT_BRD_FRMT "id:%"STR(INPUT_LEN)"[^,],won:%d,played:%d"
#define MIN(LHS, RHS) ((LHS) < (RHS) ? (LHS) : (RHS))
#ifndef NDEBUG
#define DEBUG_MSG(X) do { fputs(X, stderr); } while(0)
#define DEBUG_PRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_MSG(X)
#define DEBUG_PRINTF(...)
#endif // NDEBUG
#define LOGIN_FAIL 0
#define LOGIN_SUCCESS 1
#define LOGON 2


volatile sig_atomic_t game_suspend = 0;

static struct kread_list *texts;
static struct kread_list *users;

static struct list *board;

static size_t board_read_count;

static sem_t board_resrc_sem;
static sem_t board_read_count_sem;
static sem_t board_queue_sem;

struct usr {
    char id[INPUT_LEN];
    char password[INPUT_LEN];
    int fd;
    atomic_bool logon;
};

struct game {
    unsigned max_guess;
    unsigned guessed;
    size_t answer1_len;
    size_t answer2_len;
    char answer1[BUFF_LEN];
    char answer2[BUFF_LEN];
    char guessed_word1[BUFF_LEN];
    char guessed_word2[BUFF_LEN];
    char guessed_char[26];
    bool completed;
};

struct board_unit {
    char id[INPUT_LEN];
    int won;
    int played;
};

typedef union usr_or_board_unit {
    struct usr user;
    struct board_unit mark;
} user_t;

typedef user_t user_result_t;


/// client functions

static enum result_t play_hangman(struct game *gpack);
static void setup_game(struct game *gpack, const char *text);
static void update_game(struct game *gpack, const char guess);

/////////////////////////////////////////////////////////////

/// server functions

static bool insert_user(user_t *user);
static void clear_users(void);
static int usrcmp(void*, void*);
static void* validate_acc(const char *id, const char *password);
static void load_authentication(const char*);
static void load_texts(const char*);
static void clear_texts(void);
static const char *pick_text(void);
static void create_board(void);
static void clear_board(void);
static int ranking_cmp(void*, void*);
static void board_read_enter_block(void);
static void board_read_exit_block(void);
static void board_write_enter_block(void);
static void board_write_exit_block(void);

////////////////////////////////////////////////////////////


void server_init(void) {
    srand((unsigned) time(0));
    load_authentication("Authentication.txt");
    load_texts("hangman_text.txt");
    create_board();
}

void *login(const int fd) {
    char id[INPUT_LEN], password[INPUT_LEN];
    int login_status = LOGIN_FAIL;
    char msg[SHRT_BUFF];

    if (recv(fd, id, INPUT_LEN, 0) == -1) {
        perror("receiving id failed.");
        return NULL;
    }
    if (game_suspend) {
        fputs("SIGINT captured. login aborted.\n", stderr);
        return NULL;
    }
    DEBUG_PRINTF("received id: %s\n", id);

    if (recv(fd, password, INPUT_LEN, 0) == -1) {
        perror("receiving password failed.");
        return NULL;
    }
    if (game_suspend) {
        fputs("SIGINT captured. login aborted.\n", stderr);
        return NULL;
    }
    DEBUG_PRINTF("received password: %s\n", password);

    struct usr *userp = validate_acc(id, password);
    if (userp) {
        if (userp->logon) {
            login_status = LOGON;
        } else {
            userp->fd = fd;
            userp->logon = true;
            login_status = LOGIN_SUCCESS;
        }
    }
    sprintf(msg, "%d", login_status);
    send(fd, &msg, sizeof(char) * SHRT_BUFF, 0);

    return userp;
}

int usr_get_fd(void *userp) {
    struct usr *user = userp;

    return user->fd;
}

void usr_logout(void *userp) {
    struct usr *user = userp;
    user->logon = false;
}

enum menu_input server_interpret(const char *input) {
    int get;

    sscanf(input, "%d", &get);
    
    return (enum menu_input) get;
}

void server_menu(void *userp) {
    struct usr *user = userp;
    char buff[SHRT_BUFF];

    bool quit = false;

    do {
        if (recv(user->fd, buff, SHRT_BUFF, 0) == -1) {
            perror("receiving client input failed.");
        } else {
            enum result_t result;
            enum menu_input input = server_interpret(buff);
            switch (input) {
            case PLY:
                result = server_play(userp);
                update_board(userp, result);
                if (result == RESULT_ERR)
                    quit = true;
                break;
            case BRD:
                server_board(userp);
                break;
            case QUIT:
                quit = true;
                break;
            }
        }
        if (game_suspend)
            fprintf(stderr, "SIGINT received. Aborting...\n");
    } while (!quit && !game_suspend);
    usr_logout(userp);
}

enum result_t server_play(void *userp) {
    struct usr* user = userp;
    int fd = user->fd;
    char buff[SHRT_BUFF];
    int id_won, id_game;
    
    puts("Entered server_play");
    const char *text = pick_text();
    if (text)
        puts(text);
    else
        puts("NULL text?!");
    if (send(fd, text, BUFF_LEN, 0) == -1) {
        perror("sending text failed.");
        return RESULT_ERR;
    }

    if (recv(fd, buff, SHRT_BUFF, 0) == -1) {
        perror("receiving result_t failed.");
        return RESULT_ERR;
    }
    if (game_suspend)
        return RESULT_ERR;

    return buff[0] == '1' ? WIN : LOSE;
}

void server_board(void *userp) {
    char buff[BUFF_LEN];
    int fd = ((struct usr*) userp)->fd;
    sprintf(buff, "%lu", board->size);
    if (send(fd, buff, BUFF_LEN, 0) == -1) {
        perror("sending board size failed.");
    }

    board_read_enter_block();

    for (struct list_node *curr = board->head; curr; curr = curr->next) {
        user_result_t  *result = curr->data;
        sprintf(buff, SERVER_BRD_FRMT, result->mark.id, result->mark.won, result->mark.played);
        if (send(fd, buff, BUFF_LEN, 0) == -1) {
            perror("sending leaderboard failed.");
            return;
        }
    }

    board_read_exit_block();
        
}

void update_board(void *userp, enum result_t result) {
    board_write_enter_block();
    user_t *user = userp;

    user_result_t *user_history = list_find(board, userp, usrcmp);
    if (!user_history) {
        user_history = malloc(sizeof(user_result_t));
        memcpy(user_history->mark.id, user->user.id, sizeof(char) * INPUT_LEN);
        user_history->mark.won = user_history->mark.played = 0;
        list_add(board, user_history);
    }

    if (result != RESULT_ERR) {
        ++user_history->mark.played;
        if (result == WIN)
            ++user_history->mark.won;
    }

    board_write_exit_block();
}

void server_cleanup(void) {

    clear_texts();
    clear_users();
    clear_board();

}

void *client_login(int sock_fd) {
    struct usr *user = NULL;
    char id[INPUT_LEN], password[INPUT_LEN];
    char msg[SHRT_BUFF];
    int login_status;

    printf("Please enter your username-->");
    fgets(id, INPUT_LEN, stdin);
    id[strlen(id) - 1] = '\0';
    puts(id);
    printf("Please enter your password-->");
    fgets(password, INPUT_LEN, stdin);   
    password[strlen(password) - 1] = '\0';
    puts(password);

    if (send(sock_fd, id, sizeof(char) * INPUT_LEN, 0) == -1)
        goto login_err;

    if (send(sock_fd, password, sizeof(char) * INPUT_LEN, 0) == -1)
        goto login_err;

    if (recv(sock_fd, msg, sizeof(char) * SHRT_BUFF, 0) == -1)
        goto login_err;

    sscanf(msg, "%d", &login_status);

    switch (login_status) {
    case LOGIN_SUCCESS:
        puts("The combination you entered is correct -- Entering the game...");
        user = malloc(sizeof(struct usr));
        memcpy(user->id, id, sizeof(char) * INPUT_LEN);
        user->fd = sock_fd;
        break;
    case LOGIN_FAIL:
        puts("Either username or password is incorrect -- disconnecting...");
        break;
    case LOGON:
        puts("Invalid Operation: This account has been logon.");
        break;
    }

    return user;


login_err:
    perror("Connection broken."); 

    return NULL;
}

enum result_t client_play(void *userp) {
    char text_buff[BUFF_LEN];
    char answer1[BUFF_LEN], answer2[BUFF_LEN];
    char buff[SHRT_BUFF];

    sprintf(buff, "%d", PLY);

    struct usr *user = userp;
    int sock_fd = user->fd;

    if (send(sock_fd, buff, sizeof(char) * SHRT_BUFF, 0) == -1) {
        perror("sending menu instruction failed.");
        return RESULT_ERR;
    }

    if (recv(sock_fd, text_buff, sizeof(char) * BUFF_LEN, 0) == -1) {
        perror("receiving text from server failed.");
        return RESULT_ERR;
    }

    struct game gpack;
    setup_game(&gpack, text_buff);

    return play_hangman(&gpack);
}

void client_board(void *userp) {
    char buff[SHRT_BUFF];
    struct usr *user = userp;
    size_t board_size;

    sprintf(buff, "%d", (int) BRD);

    if (send(user->fd, buff, sizeof(char) * SHRT_BUFF, 0) == -1) {
        perror("Requesting leaderboard failed.");
        return;
    }

    if (recv(user->fd, buff, sizeof(char) * BUFF_LEN, 0) == -1) {
        perror("Receiving size of leaderboard failed.");
        return;
    }
    sscanf(buff, "%lu", &board_size);

    if (board_size == 0) {
        puts("\n=============================================================================\n");
        puts("There is no information currently stored in the Leader Board. Try again later");
        puts("\n=============================================================================\n");

    }

    for (size_t i = 0; i != board_size; ++i) {
        char user_result[BUFF_LEN];
        char id[INPUT_LEN];
        int won, played;

        if (recv(user->fd, user_result, sizeof(char) * BUFF_LEN, 0) == -1) {
            perror("Receiving leaderboard failed.");
            return;
        }

        sscanf(user_result, CLIENT_BRD_FRMT, id, &won, &played);

        puts("\n=====================================================================\n");

        printf("Player\t- %s\n", id);
        printf("Number of games won\t- %d\n", won);
        printf("Number of games played\t- %d\n", played);

        puts("\n=====================================================================\n");

    }
    
}

void client_quit(void *userp) {
    struct usr *user = userp;
    char buff[SHRT_BUFF];

    sprintf(buff, "%d", (int) QUIT);

    if (send(user->fd, buff, sizeof(char) * SHRT_BUFF, 0) == -1) {
        perror("Sending quit signal failed.");
    }
}

void client_logout(void *userp) {
    char buff[SHRT_BUFF];

    struct usr *user = userp;

    sprintf(buff, "%d", (int) QUIT);
    if (send(user->fd, buff, sizeof(char) * SHRT_BUFF, 0) == -1) {
        perror("sending menu instruction failed.");
    }

    free(user);
}

void client_send_result(void *userp, enum result_t result) {
    struct usr *user = userp;
    char buff[1];

    buff[0] = result == WIN ? '1' : '0';
    if (send(user->fd, buff, sizeof(char), 0) == -1) {
        perror("send gaming result failed.");
    }
}

static enum result_t play_hangman(struct game *gpack) {
    enum result_t ret_val;
    char guess;

    unsigned guess_left = 0;

    
    while (!gpack->completed && (guess_left = gpack->max_guess - gpack->guessed)) {
        printf("Guessed letters: %s\n", gpack->guessed_char);
        printf("Number of guesses left: %u\n", guess_left);
        putchar('\n');
        printf("Word: %s\t%s\n", gpack->guessed_word1, gpack->guessed_word2);

        printf("\nEnter your guess -> ");
        
        guess = getchar();
        printf("\n=====================================================================\n");
        update_game(gpack, guess);
        while ((guess = getchar()) != '\n' && guess != EOF)
            continue;
    }

    if (gpack->completed) {
        puts("Game Over!\nWell Done!, You have won this round of Hangman!");
        ret_val = WIN;
    } else {
        puts("Game Over!\nBad Luck!, You have run out of guesses. The Hangman got you!");
        ret_val = LOSE;
    }

    return ret_val;
}

static void setup_game(struct game *gpack, const char *text) {
    char pattern[BUFF_LEN];

    sprintf(pattern, "%%%d[^,],%%s", INPUT_LEN);
    
    sscanf(text, pattern, gpack->answer1, gpack->answer2);


    gpack->answer1_len = strlen(gpack->answer1);
    gpack->answer2_len = strlen(gpack->answer2);
    printf("%s, %lu, %lu\n", text, gpack->answer1_len, gpack->answer2_len);
    for(size_t i = 0; i < gpack->answer1_len; ++i)
        gpack->guessed_word1[i] = '_';
    gpack->guessed_word1[gpack->answer1_len] = '\0';

    for(size_t i = 0; i < gpack->answer2_len; ++i)
        gpack->guessed_word2[i] = '_';
    gpack->guessed_word2[gpack->answer2_len] = '\0';

    //The guesses made bar set to NULL before game start
    gpack->guessed_char[0] = '\0';
    gpack->guessed = 0;
    gpack->completed = false;
    gpack->max_guess = MIN(gpack->answer1_len + gpack->answer2_len + 10u, 26);
}

static void update_game(struct game *gpack, const char guess) {
    gpack->guessed_char[gpack->guessed++] = guess;
    gpack->guessed_char[gpack->guessed] = '\0';

    for (size_t i = 0; i != gpack->answer1_len; ++i) {
        if (guess == gpack->answer1[i]) {
            gpack->guessed_word1[i] = guess;
        }
    }

    for (size_t i = 0; i != gpack->answer2_len; ++i) {
        if (guess == gpack->answer2[i]) {
            gpack->guessed_word2[i] = guess;
        }
    }

    bool word1_completed = true, word2_completed = true;
    for (size_t i = 0; i != gpack->answer1_len; ++i) {
        if (gpack->guessed_word1[i] == '_') {
            word1_completed = false;
            break;
        }
    }
    for (size_t i = 0; i != gpack->answer2_len; ++i) {
        if (gpack->guessed_word2[i] == '_') {
            word2_completed = false;
            break;
        }
    }

    gpack->completed =  word1_completed && word2_completed;


    //sleep(2);
}

static void clear_users(void) {
    for (size_t i = 0; i != users->size; ++i) {
        free(kread_list_get(users, i));
    }
    kread_list_destroy(users);
}

static int usrcmp(void *lhs, void *rhs) {
    user_result_t *ub_l = lhs, *ub_r = rhs;
    return strcmp(ub_l->mark.id, ub_r->mark.id);
}

static void* validate_acc(const char *id, const char *pwd) {
    void* userp = NULL;

    for (size_t i = 0; i != users->size; ++i) {
        user_t *curr = kread_list_get(users, i);
        if (strcmp(id, curr->user.id) == 0 && strcmp(pwd, curr->user.password) == 0) {
            userp = &curr->user;
            break;
        }
    }

    return userp;
}
            
static void load_authentication(const char *path) {
    users = kread_list_create();
    FILE *fp = fopen(path, "r");
    char buff[BUFF_LEN];

    // skip first line
    fgets(buff, BUFF_LEN, fp);

    while(fgets(buff, BUFF_LEN, fp)) {
        user_t *new_user = malloc(sizeof(user_t));
         
        new_user->user.logon = false;

        struct usr *user = &new_user->user;

        // retrieve id and password
        if (sscanf(buff, "%s %s", user->id, user->password) != 2)
            fputs("sscanf format error\n", stderr);
        
        kread_list_add(users, new_user);
    }

    fclose(fp);
}

static void load_texts(const char *path) {
    texts = kread_list_create();
    FILE *fp = fopen(path, "r");
    char *buff = malloc(sizeof(char) * BUFF_LEN);

    fgets(buff, BUFF_LEN, fp);

    while(fgets(buff, BUFF_LEN, fp)) {
        char *text = malloc(sizeof(char) * BUFF_LEN);
        memcpy(text, buff, BUFF_LEN);
        text[strlen(text) - 1] = '\0';
        kread_list_add(texts, text);
    }

    fclose(fp);
}

static void clear_texts(void) {
    for (size_t i = 0; i != texts->size; ++i)
        free(kread_list_get(texts, i));
    kread_list_destroy(texts);
}

static const char *pick_text(void) {
    size_t index = rand() % texts->size;

    return kread_list_get(texts, index);
}

static void create_board(void) {
    board = list_create(ranking_cmp);

    board_read_count = 0;
    sem_init(&board_resrc_sem, 1);
    sem_init(&board_read_count_sem, 1);
    sem_init(&board_queue_sem, 1);
}

static void clear_board(void) {
    list_clear(board);
    list_destroy(board);
}

static int ranking_cmp(void *lhs, void *rhs) {
    user_result_t *l = lhs, *r = rhs;

    int won_val = l->mark.won - r->mark.won;
    if (won_val == 0)
    {
        return 0;
    } else if (won_val > 0) {
        return -1;
    } else {
        return 1;
    }

}

static inline void board_read_enter_block(void) {
    sem_wait(&board_queue_sem);
    sem_wait(&board_read_count_sem);
    if (board_read_count == 0)
        sem_wait(&board_resrc_sem);
    ++board_read_count;
    sem_post(&board_queue_sem);
    sem_post(&board_read_count_sem);
}

static inline void board_read_exit_block(void) {
    sem_wait(&board_read_count_sem);
    --board_read_count;
    if (board_read_count == 0)
        sem_post(&board_resrc_sem);
    sem_post(&board_read_count_sem);
}

static inline void board_write_enter_block(void) {
    sem_wait(&board_queue_sem);
    sem_wait(&board_resrc_sem);
    sem_post(&board_queue_sem);
}

static inline void board_write_exit_block(void) {
    sem_post(&board_resrc_sem);
}
