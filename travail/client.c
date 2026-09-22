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

int handle_connect(const char *server_family , const char *server_port){
     int fd = socket (AF_INET , SOCK_STREAM , 0); 
     die(fd , "Socket Creation");

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(server_port));
    inet_aton(server_family , &server_addr.sin_addr);

    int ret = connect(fd, (struct sockaddr*)&server_addr , sizeof(server_addr));
    die(ret , "On connecting");
    printf("Connected to server :%s on port %s", server_family , server_port);

    return fd;

}


int main(int argc , char *argv[])
{
    handle_connect(argv[1], argv[2]);

}