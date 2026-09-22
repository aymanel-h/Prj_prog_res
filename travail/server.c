#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "common.h"
#include <poll.h>



void die(int ret_value, const char *msg) {
        if (ret_value == -1) {
            perror(msg);
        }
}

int read_from_socket(int socket_fd, void* buf, int size){
    int size_read = 0;
    int ret_value = 0;
    while(size_read != size){
        ret_value = read(socket_fd, (char*)(buf)+size_read, size-size_read);
        die(ret_value, "reading from socket");
        if(ret_value == 0){
			return 0;
        }
        size_read += ret_value;
    }
    return size_read;
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


		struct pollfd fds[FD_TAB_SIZE];
        fds[0].fd = listen_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        for(int i = 1 ; i<FD_TAB_SIZE ; i++){
            fds[i].fd = -1;
            fds[i].events = 0;
            fds[i].revents = 0;
		}	
        
        while(1){
            printf("Will poll...\n");
            int nbfds = poll(fds, FD_TAB_SIZE,-1);
            printf("Number of active fd (%d)\n", nbfds);
            for(int i = 0; i<FD_TAB_SIZE ; i++){
                //if activity on listening socket
                if(i == 0 && (fds[0].revents & POLLIN)){
                    fds[i].revents = 0;
                    int new_fd = accept(fds[0].fd,NULL,NULL);
					die(new_fd,"On Accepting");
                    for (int j = 0; j<FD_TAB_SIZE ; j++){
                        // recherche de la premiere service socket libre pour y mettre le nouveau client
                        if(fds[j].fd == -1){
                            fds[j].fd = new_fd;
                            fds[j].events = POLLIN;
                            fds[j].revents = 0;
                            printf("New client fd n : %d \n",fds[j].fd);
                            break;  

                            
                        }
                        
                    }
                    printf("ok...\n");

                }
                else if( i != 0 && (fds[i].revents & POLLIN)){
                    int size_of_next_msg = 0;
                    

                    int size_read = read_from_socket(fds[i].fd,&size_of_next_msg,sizeof(int));
					if(size_read == 0){
						close(fds[i].fd);
						printf("client in fd %d disconnected", fds[i].fd);
						fds[i].fd = -1;
						fds[i].events = 0;
            			fds[i].revents = 0;
						continue ;
					}

                    die(size_read,"On reading");
                    printf("MSG size(%d) ,client n (%d)  \n",size_of_next_msg, fds[i].fd);



                    char *msg_received = malloc(size_of_next_msg * sizeof(char) +1);
					msg_received[size_of_next_msg]= '\0';
                    int nb_read = read_from_socket(fds[i].fd,msg_received,size_of_next_msg);
					if(nb_read == 0){
						free(msg_received);
						close(fds[i].fd);
						printf("client in fd %d disconnected", fds[i].fd);
						fds[i].fd = -1;
						fds[i].events = 0;
            			fds[i].revents = 0;
						continue ;
					}
                    die( nb_read,"On reading");
                    printf("MSG RECU par client de fd %d :%s, nb_d octets lus: %d",fds[i].fd ,msg_received ,nb_read );
					free(msg_received);

                }
            }

        }

		close(listen_fd);

		return EXIT_SUCCESS;
	
}

