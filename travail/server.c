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
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0;
                    delete_client(&head, fds[i].fd);
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