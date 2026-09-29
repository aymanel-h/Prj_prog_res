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



int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    die(listen_fd, "Listening socket creation");

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    inet_aton("127.0.0.1", &server_addr.sin_addr);

    int ret_value = bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    die(ret_value, "on binding");

    ret_value = listen(listen_fd, BACKLOG);
    die(ret_value, "On listening");

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
                int ret=recv_msg(fds[i].fd, &msg, &payload);
                if (ret <= 0) {
                if (ret < 0) {
                    fprintf(stderr, "Error receiving message from client\n");
                } 
                else {
                    printf("Client disconnected\n");
                }
                close(fds[i].fd);
                fds[i].fd = -1;
                fds[i].events = 0;
                fds[i].revents = 0;
                delete_client(&head, fds[i].fd);
                continue;
                }


                if (payload != NULL && strcmp(payload, "/quit") == 0) {
                    free(payload);
                    close(fds[i].fd);
                    delete_client(&head, fds[i].fd);
                    printf("[DEBUG] Client fd=%d supprimé de la liste\n",fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0;
                    continue;
                }
                
                
                if (msg.type == NICKNAME_INFOS) {
                    struct infos_client *target = get_client_by_nick(head, msg.infos);
                    char response_pld[512];

                    struct message rep;
                    memset(&rep, 0, sizeof(struct message));
                    rep.type = NICKNAME_INFOS;
                    strncpy(rep.infos, msg.infos, INFOS_LEN - 1);
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
                                 "[Server] : User '%s' does not exist.\n", msg.infos);
                    }

                    rep.pld_len = strlen(response_pld) + 1;
                    send_msg(fds[i].fd, &rep, response_pld);

                    if (payload != NULL) free(payload);
                    continue;
                }
                if (msg.type == NICKNAME_LIST) {
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

                    send_msg(fds[i].fd, &rep, response_pld);

                    if (payload != NULL) free(payload);
                    continue;
                }
                
                if (msg.type == BROADCAST_SEND) {
                    char formatted_msg[MSG_LEN + 150];
                    snprintf(formatted_msg, sizeof(formatted_msg), "[%s] : %s\n", msg.nick_sender, payload ? payload : "");

                    struct message out_msg;
                    memset(&out_msg, 0, sizeof(struct message));
                    out_msg.type = BROADCAST_SEND;
                    strncpy(out_msg.nick_sender, msg.nick_sender, NICK_LEN - 1);
                    out_msg.pld_len = strlen(formatted_msg) + 1;

                    struct infos_client *curr = head;
                    while (curr != NULL) {
                        if (curr->fd != fds[i].fd && strlen(curr->nickname) > 0) {
                            send_msg(curr->fd, &out_msg, formatted_msg);
                        }
                        curr = curr->next;
                    }

                    if (payload != NULL) free(payload);
                    continue;
                }

                if (msg.type == UNICAST_SEND) {
                    struct infos_client *dest = get_client_by_nick(head, msg.infos);

                    if (dest != NULL) {
                        char formatted_msg[MSG_LEN + 150];
                        snprintf(formatted_msg, sizeof(formatted_msg), "[%s] : %s\n", msg.nick_sender, payload);

                        struct message out_msg;
                        memset(&out_msg, 0, sizeof(struct message));
                        out_msg.type = UNICAST_SEND;
                        strncpy(out_msg.nick_sender, msg.nick_sender, NICK_LEN - 1);
                        out_msg.nick_sender[NICK_LEN - 1] = '\0';
                        out_msg.pld_len = strlen(formatted_msg) + 1;

                        send_msg(dest->fd, &out_msg, formatted_msg);
                    } else {
                        char err_pld[256];
                        snprintf(err_pld, sizeof(err_pld), "[Server] : User '%s' does not exist.\n", msg.infos);

                        struct message rep;
                        memset(&rep, 0, sizeof(struct message));
                        rep.type = UNICAST_SEND;
                        strncpy(rep.infos, msg.infos, INFOS_LEN - 1);
                        rep.infos[INFOS_LEN - 1] = '\0';
                        rep.pld_len = strlen(err_pld) + 1;

                        send_msg(fds[i].fd, &rep, err_pld);
                    }

                    if (payload != NULL) free(payload);
                    continue;
                }

                if (msg.type == NICKNAME_NEW) {
                    struct message rep;
                    memset(&rep, 0, sizeof(struct message));
                    rep.type = NICKNAME_NEW;

                    if (isnick_valid(head, msg.infos, fds[i].fd)) {
                        set_cl_nick(head, fds[i].fd, msg.infos);
                        strncpy(rep.infos, msg.infos, INFOS_LEN - 1);
                        rep.infos[INFOS_LEN - 1] = '\0';
                        char welcome[256];
                        snprintf(welcome, sizeof(welcome), "[Server] : Welcome on the chat %s\n", msg.infos);
                        rep.pld_len = strlen(welcome) + 1;
                        send_msg(fds[i].fd, &rep, welcome);
                    }

                    else {
                        // Pseudo déjà pris
                        char err_msg[] = "[Server] : Nickname already taken, please choose another one.\n";
                        rep.pld_len = strlen(err_msg) + 1;
                        rep.infos[0]='\0';
                        send_msg(fds[i].fd, &rep, err_msg);
                    }

                    if (payload != NULL){ 
                        free(payload);
                    }
                    continue; 

                }
                            


                if(msg.pld_len > 0 && payload != NULL) {
                    printf("Received from client: %s\n", payload);
                }

                send_msg(fds[i].fd, &msg, payload);

                free(payload);
            }
        }
        
    }

    delete_all_clients(&head);
    close(listen_fd);
    return EXIT_SUCCESS;
}