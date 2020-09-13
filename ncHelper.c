#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "ncHelper.h"

// Return a listening socket
int get_listener_socket(struct commandOptions cmdOps, int server) {
  int listener;     // Listening socket descriptor
  int yes = 1;      // For setsockopt() SO_REUSEADDR, below

  struct addrinfo hints, *ai, *p;
  char s[INET_ADDRSTRLEN];

  // Get us a socket and bind it
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (server == SERVER) {
    hints.ai_flags = AI_PASSIVE;
  }
  
  // Convert port to a char*
  char* port = malloc(sizeof(cmdOps.port));
  sprintf(port, "%d", cmdOps.port);
  if (getaddrinfo(cmdOps.hostname, port, &hints, &ai) != SUCCESS) {
    printf("nc: getaddrinfo: Name or service not known\n");
    free(port);
    return ERROR;
  }
  free(port);
  
  // Loop through all the results and connect to the first we can
  for(p = ai; p != NULL; p = p->ai_next) {
    if ((listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }
    if (server == SERVER) {
      // Lose the pesky "address already in use" error message
      if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == ERROR) {
        perror("setsocket");
        return ERROR;
      }

      if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
        close(listener);
        continue;
      }
    } else {
      if (connect(listener, p->ai_addr, p->ai_addrlen) == -1) {
        close(listener);
        if (cmdOps.option_v == 1) {
          printf("nc: connect to %s port %d (tcp) failed: Connection refused\n", cmdOps.hostname, cmdOps.port);
        }
        continue;
      }
    }
    break;
  }

  // If we got here, it means we didn't get bound
  if (p == NULL) {
    return ERROR;
  }

  freeaddrinfo(ai); // All done with this

  if (server == CLIENT) {
    void * addr = &(((struct sockaddr_in*)((struct sockaddr *)p->ai_addr))->sin_addr);
    inet_ntop(p->ai_family, addr, s, sizeof s);
  }

  return listener;
}