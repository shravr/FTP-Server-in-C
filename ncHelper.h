#ifndef NC_HELPER
#define NC_HELPER

#include "commonProto.h"

#define MAX_DATA_SIZE 256
#define MAX_CONNECTIONS 10
#define STDIN_FD 0

#define SUCCESS 0
#define ERROR -1

#define SERVER 1
#define CLIENT 0

int get_listener_socket(struct commandOptions cmdOps, int server);

#endif