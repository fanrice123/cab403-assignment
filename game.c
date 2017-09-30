#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#define BUFF_LEN 256

struct usr_list{
    struct usr_node *head;
    struct usr_node *tail;
} users;

struct usr_node {
    struct usr user;
    struct usr_node* next;
};

static void* validate_acc(const char *id, const char *password);

bool insert_user(struct usr* user) {
    struct usr_node* new_user = malloc(sizeof(struct usr_node));

    new_user->next = NULL;
    memcpy(&new_user->user, user, sizeof(struct usr));

    users.tail = new_user;
}

void clear_user(void) {
    struct usr_node* = user;
    while (user != users.tail) {
        struct usr_node* next = user->next;
        free(user);
        user = next;
    }
}

void init(void) {
    FILE *fp = fopen("Authentication.txt", "r");
    char buff[BUFF_LEN];

    fgets(fp, buff, BUFF_LEN);

    while(fgets(fp, buff, BUFF_LEN)) {
        struct usr new_user;

        // retrieve id and password
        if (sscanf(buff, "%s %s", &new_user.id, &new_user.password) != 2)
            fputs(stderr, "sscanf format error\n");
        
        insert_user(&new_user);
    }

    fclose(fp);
}

void* login(int fd) {
    char id[INPUT_LEN], password[INPUT_LEN];

    if (recv(fd, id, INPUT_LEN, 0) == -1) {
        perror("receiving id failed.");
        exit(EXIT_FAILURE);
    }

    if (recv(fd, password, INPUT_LEN, 0) == -1) {
        perror("receiving id failed.");
        exit(EXIT_FAILURE);
    }

    return validate_acc(id, password);
}

static void* validate_acc(const char *id, const char *pwd) {
    void* userp = NULL;

    for (struct usr_node* curr = users.head; curr != NULL; curr = curr->next) {
        if (strcmp(id, &curr->user.id) == 0 && strcmp(pwd, &curr->user.password)) {
            userp = &curr->user;
            break;
        }
    }

    return userp;
}
            
