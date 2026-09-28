#ifndef ADD_CLIENT_H
#define ADD_CLIENT_H

#include <netinet/in.h>
#include <sys/socket.h>

struct infos_client {
    int fd;
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
#endif