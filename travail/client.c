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

int create_listen_socket(char *ip_port_buffer) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(0); // Port chosen randomly by our OS

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 1) < 0) {
        close(fd);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr*)&addr, &len) == -1) {
        close(fd);
        return -1;
    }

    sprintf(ip_port_buffer, "127.0.0.1:%d", ntohs(addr.sin_port));

    return fd;
}

void run_client(int sockfd) {
struct pollfd fds[FD_TAB_SIZE];
    for (int i = 0; i < FD_TAB_SIZE; i++) {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    char current_nick[NICK_LEN] = "";
    char buffer[MSG_LEN];

    while (1) {
        int ret = poll(fds, FD_TAB_SIZE, -1);
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
            else if (strncmp(buffer, "/send ", 6) == 0) {
                if (strlen(current_nick) == 0) {
                    printf("[Client] : Please set a nickname first with /nick <name>.\n");
                    continue;
                }

                char target[INFOS_LEN] = {0};
                char file_name[MSG_LEN] = {0};

                int i = 6;
                int j = 0;
                // skip the spaces between /send and the pseudo
                while (buffer[i] == ' ') {
                    i++;
                }
                // copying the pseudo
                while (buffer[i] != ' ' && buffer[i] != '\0' && j < INFOS_LEN - 1) {
                    target[j] = buffer[i];
                    i++;
                    j++;
                }
                target[j] = '\0';

                //Same process to get the file name :D
                while (buffer[i] == ' ') {
                    i++;
                }

                j = 0;
                while (buffer[i] != '\0' && j < MSG_LEN - 1) {
                    file_name[j] = buffer[i];
                    i++;
                    j++;
                }
                file_name[j] = '\0';

                if (strlen(target) == 0 || strlen(file_name) == 0) {
                    printf("Usage: /send <nickname> <file_name>\n");
                    continue;
                }

                msg.type = FILE_REQUEST;
                msg.pld_len = strlen(file_name) + 1;
                strncpy(msg.nick_sender, current_nick, NICK_LEN - 1);
                strncpy(msg.infos, target, INFOS_LEN - 1);

                send_msg(sockfd, &msg, file_name);
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
           else if (rep.type == FILE_REQUEST) {
                printf("%s wants you to accept the transfer of the file named \"%s\". Do you accept? [Y/N]\n", rep.nick_sender, payload);
                
                char choice;
                scanf(" %c", &choice);
                // vider le buffer du clavier pour le prochain tour de boucle
                int c; while ((c = getchar()) != '\n' && c != EOF);

                struct message rep_file;
                memset(&rep_file, 0, sizeof(struct message));
                strncpy(rep_file.nick_sender, current_nick, NICK_LEN - 1);
                strncpy(rep_file.infos, rep.nick_sender, INFOS_LEN - 1);
                
                if (choice == 'Y' || choice == 'y') {
                    char ip_port[128];
                    int listen_fd = create_listen_socket(ip_port);
                    
                    if (listen_fd != -1) {
                        for (int j = 2; j < FD_TAB_SIZE; j++) {
                            if (fds[j].fd == -1) {
                                fds[j].fd = listen_fd;
                                fds[j].events = POLLIN;
                                break;
                            }
                        }
                        
                        rep_file.type = FILE_ACCEPT;
                        rep_file.pld_len = strlen(ip_port) + 1;
                        send_msg(sockfd, &rep_file, ip_port); // Le payload contient l'IP et le port
                        printf("File transfer accepted. Listening on %s...\n", ip_port);
                    }
                } else {
                    printf("File transfer rejected.\n");
                    rep_file.type = FILE_REJECT;
                    rep_file.pld_len = 0;
                    send_msg(sockfd, &rep_file, NULL);
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