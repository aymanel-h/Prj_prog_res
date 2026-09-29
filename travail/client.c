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
#include <ctype.h>


int handle_connect(const char *server_family, const char *server_port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    die(fd, "Socket Creation");

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(server_port));
    inet_aton(server_family, &server_addr.sin_addr);

    int ret = connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    die(ret, "On connecting");
    printf("Connected to server :%s on port %s\n", server_family, server_port);

    return fd;
}

void run_client(int sockfd) {
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    char current_nick[NICK_LEN] = "";
    
    
    char buffer[MSG_LEN];

    while (1) {
        int ret = poll(fds, 2, -1);
        die(ret, "poll()");

        if (fds[0].revents & POLLIN) {
            struct message msg;
            memset(&msg, 0, sizeof(struct message));

            msg.type = ECHO_SEND;
            
            memset(buffer, 0, MSG_LEN);
            if (fgets(buffer, MSG_LEN, stdin) == NULL) {
                break;
            }


            buffer[strcspn(buffer, "\n")] = 0;


            if (strcmp(buffer, "/quit") == 0) {
                msg.pld_len = strlen(buffer) + 1;
                send_msg(sockfd, &msg, buffer);
                break;
            }
            //dernier changement ajout de req2.1
            else if(strncmp(buffer, "/nick ", 6) == 0){
                msg.type = NICKNAME_NEW;
                char *nick = buffer + 6;
                if(strlen(nick)>128 || strlen(nick) == 0 ){
                    printf("nickname passed is too long pls choose a nickname under 128 charachters \n");
                    continue;
                }
                int valide = 1;
                for(int i = 0; i<strlen(nick); i++){
                    if(!isalnum((unsigned char)nick[i])){
                        printf("Nickname contains a non valid character, please retry... \n");
                        valide = 0;
                        break;
                    }
                }
                if(!valide){
                    continue;
                }
                msg.pld_len = 0;
                msg.type = NICKNAME_NEW ;
                strcpy(msg.infos, nick);
                send_msg(sockfd, &msg, NULL);
                continue;
            }
            else if(strlen(buffer) > 0) {
                int size = strlen(buffer) + 1;
                msg.pld_len = size;
                send_msg(sockfd, &msg, buffer);
            }
        }

        if (fds[1].revents & POLLIN) {
            struct message rep;
            char *payload = NULL;
            int ret=recv_msg(sockfd, &rep, &payload);
            if (ret <= 0) {
                printf("Server disconnected\n");
                break;
            };
            if (payload != NULL){
                printf("Received from server: %s\n", payload);
            }
           if (rep.type == NICKNAME_NEW) {
                if (strlen(rep.infos) > 0) {
                    strncpy(current_nick, rep.infos, NICK_LEN - 1);
                    current_nick[NICK_LEN - 1] = '\0';
                }
            }
            
            free(payload);
        }
    }
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int sockfd = handle_connect(argv[1], argv[2]);
    run_client(sockfd);
    close(sockfd);
    return EXIT_SUCCESS;
}