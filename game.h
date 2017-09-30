#ifndef GAME_H
#define GAME_H
#define INPUT_LEN 128

struct usr {
    char id[INPUT_LEN];
    char password[INPUT_LEN];
};

extern struct usr_list users;

void* login(int fd);

#endif // GAME_H
