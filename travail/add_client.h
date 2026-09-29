#ifndef ADD_CLIENT_H
#define ADD_CLIENT_H

#include <netinet/in.h>
#include <sys/socket.h>

struct infos_client {
    int fd;
    char nickname[INFOS_LEN];
    struct sockaddr_in client_addr;
    struct infos_client *next;
};


void add_client(struct infos_client **head, int fd, struct sockaddr_in client_addr) {
    struct infos_client *new_client = malloc(sizeof(struct infos_client));
    if (new_client == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    new_client->fd = fd;
    new_client->client_addr = client_addr;
    memset(new_client->nickname, 0, sizeof(new_client->nickname));
    new_client->next = *head;
    *head = new_client;
}

void delete_client(struct infos_client **head, int fd) {
    struct infos_client *current = *head;
    struct infos_client *prev = NULL;

    while (current != NULL) {
        if (current->fd == fd) {
            if (prev == NULL) {
                *head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

void delete_all_clients(struct infos_client **head) {
    struct infos_client *current = *head;
    struct infos_client *next;

    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    *head = NULL;
}

int isnick_valid(struct infos_client *head, const char *nick, int curr_fd) {
    struct infos_client *curr = head;
    while (curr != NULL) {
        
        if (curr->fd != curr_fd && strlen(curr->nickname) > 0) {
            if (strcmp(curr->nickname, nick) == 0) {
                return 0; 
            }
        }
        curr = curr->next;
    }
    return 1;
}


void set_cl_nick(struct infos_client *head,int fd,const char *nick){
    struct infos_client *curr=head;
    while(curr!=NULL){
        if(curr->fd==fd){
            strncpy(curr->nickname,nick,INFOS_LEN);
            curr->nickname[INFOS_LEN-1]='\0';
            return;
        }
        curr=curr->next;
    }
}
#endif