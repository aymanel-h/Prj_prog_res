#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include "common.h"
#include "add_client.h"

// factorisation avec des fonctions pour rendre le code lisible

int server_init(const char *port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    die(listen_fd, "Listening socket creation");

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    inet_aton("127.0.0.1", &server_addr.sin_addr);

    int ret_value = bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    die(ret_value, "on binding");

    ret_value = listen(listen_fd, BACKLOG);
    die(ret_value, "On listening");

    return listen_fd;
}

void handle_nickname_new(struct infos_client **head, struct message *msg, int client_fd) {
    struct message rep;
    memset(&rep, 0, sizeof(struct message));
    rep.type = NICKNAME_NEW;

    if (isnick_valid(*head, msg->infos, client_fd)) {
        set_cl_nick(*head, client_fd, msg->infos);
        strncpy(rep.infos, msg->infos, INFOS_LEN - 1);
        rep.infos[INFOS_LEN - 1] = '\0';

        char welcome[256];
        snprintf(welcome, sizeof(welcome), "[Server] : Welcome on the chat %s\n", msg->infos);
        rep.pld_len = strlen(welcome) + 1;
        send_msg(client_fd, &rep, welcome);
    } 
    else {
        char err_msg[] = "[Server] : Nickname already taken, please choose another one.\n";
        rep.pld_len = strlen(err_msg) + 1;
        rep.infos[0] = '\0';
        send_msg(client_fd, &rep, err_msg);
    }
}

void handle_who_request(struct infos_client *head, int client_fd) {
    char response_pld[4096] = "[Server] : Online users are\n";
    struct infos_client *curr = head;
    while (curr != NULL) {
        if (strlen(curr->nickname) > 0) {
            strcat(response_pld, " - ");
            strcat(response_pld, curr->nickname);
            strcat(response_pld, "\n");
        }
        curr = curr->next;
    }

    struct message rep;
    memset(&rep, 0, sizeof(struct message));
    rep.type = NICKNAME_LIST;
    rep.pld_len = strlen(response_pld) + 1;
    send_msg(client_fd, &rep, response_pld);
}

void handle_whois_request(struct infos_client *head, struct message *msg, int client_fd) {
    struct infos_client *target = get_client_by_nick(head, msg->infos);
    char response_pld[512];

    struct message rep;
    memset(&rep, 0, sizeof(struct message));
    rep.type = NICKNAME_INFOS;
    strncpy(rep.infos, msg->infos, INFOS_LEN - 1);
    rep.infos[INFOS_LEN - 1] = '\0';

    if (target != NULL) {
        snprintf(response_pld, sizeof(response_pld),
                 "[Server] : %s connected since %s with IP address %s and port number %d\n",
                 target->nickname,
                 target->connected_since,
                 inet_ntoa(target->client_addr.sin_addr),
                 ntohs(target->client_addr.sin_port));
    } else {
        snprintf(response_pld, sizeof(response_pld),
                 "[Server] : User '%s' does not exist.\n", msg->infos);
    }

    rep.pld_len = strlen(response_pld) + 1;
    send_msg(client_fd, &rep, response_pld);
}

void handle_broadcast(struct infos_client *head, struct message *msg, const char *payload, int sender_fd) {
    char formatted_msg[MSG_LEN + 150];
    snprintf(formatted_msg, sizeof(formatted_msg), "[%s] : %s\n", msg->nick_sender, payload ? payload : "");

    struct message out_msg;
    memset(&out_msg, 0, sizeof(struct message));
    out_msg.type = BROADCAST_SEND; 
    strncpy(out_msg.nick_sender, msg->nick_sender, NICK_LEN - 1);
    out_msg.nick_sender[NICK_LEN - 1] = '\0';
    out_msg.pld_len = strlen(formatted_msg) + 1;

    struct infos_client *curr = head;
    while (curr != NULL) {
        if (curr->fd != sender_fd && strlen(curr->nickname) > 0) {
            send_msg(curr->fd, &out_msg, formatted_msg);
        }
        curr = curr->next;
    }
}

void handle_unicast(struct infos_client *head, struct message *msg, const char *payload, int sender_fd) {
    struct infos_client *dest = get_client_by_nick(head, msg->infos);

    if (dest != NULL) {
        char formatted_msg[MSG_LEN + 150];
        snprintf(formatted_msg, sizeof(formatted_msg), "[%s] : %s\n", msg->nick_sender, payload ? payload : "");

        struct message out_msg;
        memset(&out_msg, 0, sizeof(struct message));
        out_msg.type = UNICAST_SEND;
        strncpy(out_msg.nick_sender, msg->nick_sender, NICK_LEN - 1);
        out_msg.nick_sender[NICK_LEN - 1] = '\0';
        out_msg.pld_len = strlen(formatted_msg) + 1;

        send_msg(dest->fd, &out_msg, formatted_msg);
    } else {
        char err_pld[256];
        snprintf(err_pld, sizeof(err_pld), "[Server] : User '%s' does not exist.\n", msg->infos);

        struct message rep;
        memset(&rep, 0, sizeof(struct message));
        rep.type = UNICAST_SEND;
        strncpy(rep.infos, msg->infos, INFOS_LEN - 1);
        rep.infos[INFOS_LEN - 1] = '\0';
        rep.pld_len = strlen(err_pld) + 1;

        send_msg(sender_fd, &rep, err_pld);
    }
}

void handle_file_request(struct infos_client *head, struct message *msg, const char *payload, int sender_fd) {
    
    struct infos_client *dest = get_client_by_nick(head, msg->infos);

    if (dest != NULL) {
        struct message out_msg;
        memset(&out_msg, 0, sizeof(struct message));
        out_msg.type = FILE_REQUEST;
        strncpy(out_msg.nick_sender, msg->nick_sender, NICK_LEN - 1);
        out_msg.nick_sender[NICK_LEN - 1] = '\0';
        out_msg.pld_len = strlen(payload) + 1;

        send_msg(dest->fd, &out_msg, payload);
    } 
}

void handle_file_response(struct infos_client *head, struct message *msg, const char *payload, int sender_fd) {
    struct infos_client *dest = get_client_by_nick(head, msg->infos);

    if (dest != NULL) {
        struct message out_msg;
        memset(&out_msg, 0, sizeof(struct message));
        out_msg.type = msg->type; // FILE_ACCEPT or FILE_REJECT
        strncpy(out_msg.nick_sender, msg->nick_sender, NICK_LEN - 1);
        out_msg.nick_sender[NICK_LEN - 1] = '\0';
        out_msg.pld_len = strlen(payload) + 1;

        send_msg(dest->fd, &out_msg, payload);
    } 
}

void dispatch_msg(struct infos_client **head, struct message *msg, char *payload, int client_fd) {
    switch (msg->type) {
        case NICKNAME_NEW:
            handle_nickname_new(head, msg, client_fd);
            break;

        case NICKNAME_LIST:
            handle_who_request(*head, client_fd);
            break;

        case NICKNAME_INFOS:
            handle_whois_request(*head, msg, client_fd);
            break;

        case BROADCAST_SEND:
            handle_broadcast(*head, msg, payload, client_fd);
            break;

        case UNICAST_SEND:
            handle_unicast(*head, msg, payload, client_fd);
            break;
        case FILE_REQUEST:
            handle_file_request(*head, msg, payload, client_fd);
            break;
        case FILE_ACCEPT | FILE_REJECT:
            handle_file_response(*head, msg, payload, client_fd);
            break;

        case ECHO_SEND:
        default:
            if (msg->pld_len > 0 && payload != NULL) {
                printf("Received from client: %s\n", payload);
            }
            send_msg(client_fd, msg, payload);
            break;
    }
}

void server_loop(const char *port) {
    int listen_fd = server_init(port);
    struct infos_client *head = NULL;

    struct pollfd fds[FD_TAB_SIZE];
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    for (int i = 1; i < FD_TAB_SIZE; i++) {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    while (1) {
        int nbfds = poll(fds, FD_TAB_SIZE, -1);
        die(nbfds, "poll()");

        for (int i = 0; i < FD_TAB_SIZE; i++) {
            if (i == 0 && (fds[0].revents & POLLIN)) {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                fds[0].revents = 0;
                int new_fd = accept(fds[0].fd, (struct sockaddr *)&client_addr, &addr_len);
                die(new_fd, "On Accepting");

                add_client(&head, new_fd, client_addr);
                printf("New client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                for (int j = 0; j < FD_TAB_SIZE; j++) {
                    if (fds[j].fd == -1) {
                        fds[j].fd = new_fd;
                        fds[j].events = POLLIN;
                        fds[j].revents = 0;
                        break;
                    }
                }
            }
            else if (i != 0 && (fds[i].revents & POLLIN)) {
                fds[i].revents = 0;
                struct message msg;
                char *payload = NULL;
                int ret = recv_msg(fds[i].fd, &msg, &payload);

                if (ret <= 0) {
                    if (ret < 0) {
                        fprintf(stderr, "Error receiving message from client\n");
                    } else {
                        printf("Client disconnected\n");
                    }
                    close(fds[i].fd);
                    delete_client(&head, fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0;
                    continue;
                }

                if (payload != NULL && strcmp(payload, "/quit") == 0) {
                    free(payload);
                    close(fds[i].fd);
                    delete_client(&head, fds[i].fd);
                    printf("[DEBUG] Client fd=%d supprimé de la liste\n", fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0;
                    continue;
                }

                dispatch_msg(&head, &msg, payload, fds[i].fd);

                if (payload != NULL) {
                    free(payload);
                }
            }
        }
    }

    delete_all_clients(&head);
    close(listen_fd);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    server_loop(argv[1]);
    return EXIT_SUCCESS;
}