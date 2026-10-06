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

int handle_connect(const char *server_name, const char *server_port) {
    struct addrinfo hints, *result, *rp;
    int sockfd;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(server_name, server_port, &hints, &result);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            continue;
        }
        
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }

        close(sockfd);
    }

    if (rp == NULL) {
        fprintf(stderr, "Impossible de se connecter à %s:%s\n", server_name, server_port);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result); // Libération de la mémoire allouée par getaddrinfo
    
    printf("Connected to server :%s on port %s\n", server_name, server_port);
    return sockfd;
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
            
            else if (strncmp(buffer, "/nick ", 6) == 0) {
                char *nick = buffer + 6;
                if (strlen(nick) >= 128 || strlen(nick) == 0) {
                    printf("nickname passed is too long pls choose a nickname under 128 charachters \n");
                    continue;
                }
                int valide = 1;
                for (int i = 0; i < strlen(nick); i++) {
                    if (!isalnum((unsigned char)nick[i])) {
                        printf("Nickname contains a non valid character, please retry... \n");
                        valide = 0;
                        break;
                    }
                }
                if (!valide) {
                    continue;
                }
                msg.pld_len = 0;
                msg.type = NICKNAME_NEW;
                strcpy(msg.infos, nick);
                if (strlen(current_nick) > 0) {
                    strncpy(msg.nick_sender, current_nick, NICK_LEN - 1);
                    msg.nick_sender[NICK_LEN - 1] = '\0';
                }
                send_msg(sockfd, &msg, NULL);
                continue;
            }

            else if (strcmp(buffer, "/who") == 0) {
                msg.type = NICKNAME_LIST;
                msg.pld_len = 0;
                msg.infos[0] = '\0';
                if (strlen(current_nick) > 0) {
                    strncpy(msg.nick_sender, current_nick, NICK_LEN - 1);
                }
                send_msg(sockfd, &msg, NULL);
                continue;
            }

            else if (strncmp(buffer, "/whois ", 7) == 0) {
                char *target = buffer + 7;
                while (*target == ' ') target++; 

                if (strlen(target) == 0) {
                    printf("Usage: /whois <nickname>\n");
                    continue;
                }

                msg.type = NICKNAME_INFOS;
                msg.pld_len = 0;
                strncpy(msg.infos, target, INFOS_LEN - 1);
                msg.infos[INFOS_LEN - 1] = '\0';
                if (strlen(current_nick) > 0) {
                    strncpy(msg.nick_sender, current_nick, NICK_LEN - 1);
                }
                send_msg(sockfd, &msg, NULL);
                continue;
            }

            else if (strncmp(buffer, "/msgall ", 8) == 0) {
                if (strlen(current_nick) == 0) {
                    printf("[Client] : Please set a nickname first with /nick <name>.\n");
                    continue;
                }
                char *text = buffer + 8;
                while (*text == ' ') text++;
                if (strlen(text) == 0) {
                    printf("Usage: /msgall <message>\n");
                    continue;
                }

                msg.type = BROADCAST_SEND;
                msg.infos[0] = '\0';
                strncpy(msg.nick_sender, current_nick, NICK_LEN - 1);
                msg.pld_len = strlen(text) + 1;
                send_msg(sockfd, &msg, text);
                continue;
            }

            else if (strncmp(buffer, "/msg ", 5) == 0) {
                if (strlen(current_nick) == 0) {
                    printf("[Client] : Please set a nickname first with /nick <name>.\n");
                    continue;
                }

                char *target = buffer + 5;
                char *space = strchr(target, ' ');

                if (space == NULL) {
                    printf("Usage: /msg <nickname> <message>\n");
                    continue;
                }

                *space = '\0';            
                char *text = space + 1;   

                msg.type = UNICAST_SEND;
                strncpy(msg.infos, target, INFOS_LEN - 1);
                msg.infos[INFOS_LEN - 1] = '\0';

                strncpy(msg.nick_sender, current_nick, NICK_LEN - 1);
                msg.nick_sender[NICK_LEN - 1] = '\0';

                msg.pld_len = strlen(text) + 1;
                send_msg(sockfd, &msg, text);
                continue;
            }
            else if(strncmp(buffer,"/send",5) == 0) {
                if (strlen(current_nick) == 0) {
                    printf("[Client] : Please set a nickname first with /nick <name>.\n");
                    continue;
                }
                char *text = buffer + 5;
                while (*text == ' ') text++;
                if (strlen(text) == 0) {
                    printf("Usage: /send <nickname> <message>\n");
                    continue;
                }
                
                msg.type = FILE_REQUEST;
                msg.pld_len = strlen(text) + 1;
                strcpy(msg.nick_sender, current_nick);
                strcpy(msg.infos, current_nick);
                send_msg(sockfd, &msg, text);
                
                continue;
            }

            else if (strlen(buffer) > 0) {
                if (strlen(current_nick) == 0) {
                    printf("[Client] : You must choose a nickname with /nick <name> before chatting!\n");
                    continue;
                }
                int size = strlen(buffer) + 1;
                msg.pld_len = size;
                strncpy(msg.nick_sender, current_nick, NICK_LEN - 1);
                send_msg(sockfd, &msg, buffer);
            }
        }

        if (fds[1].revents & POLLIN) {
            struct message rep;
            char *payload = NULL;
            int ret = recv_msg(sockfd, &rep, &payload);
            if (ret <= 0) {
                printf("Server disconnected\n");
                break;
            }
            if (payload != NULL) {
                printf("Received from server: %s\n", payload);
            }
            if (rep.type == NICKNAME_NEW) {
                if (strlen(rep.infos) > 0) {
                    strncpy(current_nick, rep.infos, NICK_LEN - 1);
                    current_nick[NICK_LEN - 1] = '\0';
                }
            }
            else if(rep.type==FILE_REQUEST){
                char *i=strchr(payload,' ');
                printf("user1 wants you to accept the transfer of the file named %s. Do you accept? [Y/N]\n", i+1);
                char choice;
                scanf(" %c", &choice);
                if (choice == 'Y' || choice == 'y') {
                    printf("File transfer accepted.\n");
                    struct message rep_accept;
                    rep_accept.type = FILE_ACCEPT   ;
                    send_msg(sockfd, &rep_accept, "accepted");
                } else {
                    printf("File transfer rejected.\n");
                    struct message rep_reject;
                    rep_reject.type = FILE_REJECT;
                    send_msg(sockfd, &rep_reject, "rejected");
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