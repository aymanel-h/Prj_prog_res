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

    char buffer[MSG_LEN];

    while (1) {
        int ret = poll(fds, 2, -1);
        die(ret, "poll()");

        if (fds[0].revents & POLLIN) {
            memset(buffer, 0, MSG_LEN);
            if (fgets(buffer, MSG_LEN, stdin) == NULL) {
                break;
            }
            buffer[strcspn(buffer, "\n")] = 0;

            if (strcmp(buffer, "/quit") == 0) {
                int size = 6;
                write_on_socket(sockfd, &size, sizeof(int));
                write_on_socket(sockfd, buffer, size);
                break;
            }

            if (strlen(buffer) > 0) {
                int size = strlen(buffer) + 1;
                write_on_socket(sockfd, &size, sizeof(int));
                write_on_socket(sockfd, buffer, size);
            }
        }

        if (fds[1].revents & POLLIN) {
            int size_msg = 0;
            int ret_read = read_from_socket(sockfd, &size_msg, sizeof(int));
            if (ret_read == 0) {
                printf("Server disconnected\n");
                break;
            }

            char *msg = malloc(size_msg * sizeof(char));
            read_from_socket(sockfd, msg, size_msg);
            printf("Received from server: %s\n", msg);
            free(msg);
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