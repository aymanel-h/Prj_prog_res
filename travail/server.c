#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "common.h"



void die(int ret_value, const char *msg) {
        if (ret_value == -1) {
            perror(msg);
            exit(EXIT_FAILURE);
        }
}








int main(int argc , char *argv[]) {

	int listen_fd = socket(AF_INET,SOCK_STREAM,0);
        die(listen_fd,"Listening socket creation");
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(atoi(argv[1]));
        inet_aton("127.0.0.1",&server_addr.sin_addr);
        

        int ret_value = bind(listen_fd,(struct sockaddr *)&server_addr, sizeof(server_addr));
        die( ret_value, "on binding");

        ret_value = listen(listen_fd, BACKLOG);
        die(ret_value,"On listening");


        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(struct sockaddr_in);
        printf("Accepting...\n");
        int new_clientfd = accept(listen_fd, (struct sockaddr *)&client_addr, &addrlen);
        die(new_clientfd, "Accept");
        printf("New client on addr(%s:%hu) and fd %d\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port), new_clientfd);
	
}

