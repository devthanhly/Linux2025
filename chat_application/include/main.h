#ifndef _MAIN_H_
#define _MAIN_H_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <netinet/in.h>
#include <fcntl.h>


#define MAX_CONNECTIONS     10
#define MAX_MESSAGE         100
#define BUFFER_SIZE         1024


typedef struct 
{
    int sockfd;
    char ip[INET_ADDRSTRLEN];
    int port;
    int active;
} Connection_t;



#endif
