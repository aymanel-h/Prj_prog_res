#define MSG_LEN 1024
#define SERV_PORT "8080"
#define SERV_ADDR "127.0.0.1"
#define BACKLOG 20
#define FD_TAB_SIZE 128
#include "msg_struct.h"

struct info{
    short s;
    long l;
};


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
 
// Si ret_value < 0 : affiche l'erreur et quitte
static void die(int ret_value, const char *msg) 
{
	if (ret_value < 0) {
		perror(msg);
		exit(EXIT_FAILURE);
	}
}
 
// on s'assure qu'elle ecrit exactement size octets 
// retourne 1 si tout est envoyé et -1 si ya une erreur
int write_on_socket(int fd, const void *ptr, int size) 
{
	int written_bytes = 0;
	while (written_bytes != size) {
		int ret_value = write(fd, (char *)(ptr) + written_bytes, size - written_bytes);
		if (ret_value <= 0) {
			return -1;
		}
		written_bytes += ret_value;
	}
	return 1;
}
 
// on s'assure qu'on lit exactement size octets 
// retourne 1 si tout est lu et 0 si l'autre coté s'est deconnecte et enfin -1 si erreur
int read_from_socket(int fd, const void *ptr, int size) 
{
	int read_bytes = 0;
	while (read_bytes != size) {
		int ret_value = read(fd, (char *)(ptr) + read_bytes, size - read_bytes);
		if (ret_value == 0) {
			return 0;
		}
		if (ret_value < 0) {
			return -1;
		}
		read_bytes += ret_value;
	}
	return 1;
}


static inline int send_msg(int fd, struct message *msg, const char *payload) {
	 //send_msg function sends a message struct and its payload over the socket req2.0

    int ret = write_on_socket(fd, msg, sizeof(struct message));
    die(ret, "Error sending message length");
    if(msg->pld_len > 0 && payload != NULL) {
        ret = write_on_socket(fd, payload, msg->pld_len);
        die(ret, "Error sending message payload");
    }
    return 1;
}




static inline int recv_msg(int fd, struct message *msg, char **payload) { 

	int ret=read_from_socket(fd, msg, sizeof(struct message));
	die(ret, "Error receiving message length");
	if(msg->pld_len > 0) {
		*payload = malloc(msg->pld_len * sizeof(char));
		ret=read_from_socket(fd, *payload, msg->pld_len);
		die(ret, "Error receiving message payload");	
	}
	else{
		*payload = NULL;
	}
	return 1;

}