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
#include <poll.h>

#include "ncHelper.h"

// Add a new file descriptor to the set
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
  // If we don't have room, double the space in the pfds array
  if (*fd_count == *fd_size) {
    *fd_size *= 2;

    *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
  }

  (*pfds)[*fd_count].fd = newfd;
  (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

  ++(*fd_count);
}

// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
  // Copy the one from the end over this one
  pfds[i] = pfds[*fd_count - 1];

  --(*fd_count);
}

void push(int *queue, int *queue_count, int *queue_size, int fd) {
  if (*queue_count == *queue_size) {
    *queue_size += 2;

    queue = realloc(queue, sizeof(int) * (*queue_size));
  }

  (queue)[*queue_count] = fd;

  ++(*queue_count);
}

void pop(int *queue, int *queue_count, int queue_size) {
  // Shift elements down
  for (int j = 0; j < queue_size - 1; j++) {
    queue[j] = queue[j + 1];
  }

  --(*queue_count);
}

int server(struct commandOptions cmdOps) {
  struct sockaddr_storage remoteaddr; // Client address

  char buf[MAX_DATA_SIZE];    // Buffer for data

  char remoteIP[INET_ADDRSTRLEN];

  // Start off with room for MAX_CONNECTIONS + 2 connections
  // (We'll realloc as necessary)
  int fd_count = 0;
  int fd_size = MAX_CONNECTIONS + 2;
  struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

  // Used to store the next connection
  int queue_count = 0;
  int queue_size = 1;
  int* queue = malloc(sizeof(int) * queue_size);

  const int max_connections = cmdOps.option_r == 0 ? 1 + 2 : MAX_CONNECTIONS + 2;

  // Set up and get a listening socket descriptor
  const int listener_fd = get_listener_socket(cmdOps, SERVER);

  // Listen
  if (listen(listener_fd, 10) == ERROR) {
    return ERROR;
  }

  if (listener_fd == ERROR) {
    fprintf(stderr, "error getting listening socket\n");
    return ERROR;
  }

  // Add the listener to set
  pfds[0].fd = listener_fd;
  pfds[0].events = POLLIN; // Report ready to read on incoming connection

  pfds[1].fd = STDIN_FD;
  pfds[1].events = POLLIN;

  fd_count = 2; // For the listener

  if (cmdOps.option_v == 1) {
    if (cmdOps.hostname == NULL) {
      printf("Listening on [0.0.0.0] (family 0, port %d)\n", cmdOps.port);
    } else {
      printf("Listening on [%s] (family 0, port %d)\n", cmdOps.hostname, cmdOps.port);
    }
  }

  // Main loop
  while(1) {
    if (poll(pfds, fd_count, -1) == ERROR) {
      perror("poll");
      return ERROR;
    }

    // Run through the existing connections looking for data to read
    for(int i = 0; i < fd_count; i++) {
      // Check if someone's ready to read
      // stdin
      if ((pfds[i].revents & POLLIN) && pfds[i].fd == STDIN_FD) {
        memset(buf, 0, MAX_DATA_SIZE - 1);
        int nbytes;
        if ((nbytes = read(pfds[i].fd, buf, sizeof buf)) == ERROR) {
          perror("read");
          continue;
        }
        for(int j = 0; j < fd_count; j++) {
          // Send to everyone!
          const int dest_fd = pfds[j].fd;
          // Except the listener and ourselves
          if (dest_fd != listener_fd && dest_fd != STDIN_FD) {
            if (send(dest_fd, buf, nbytes, 0) == ERROR) {
              perror("send");
            }
          }
        }
      // Listener
      } else if ((pfds[i].revents & POLLIN) && pfds[i].fd == listener_fd) {
        // If listener is ready to read, handle new connection
        socklen_t addrlen = sizeof remoteaddr;
        int newfd;
        if ((newfd = accept(listener_fd, (struct sockaddr *)&remoteaddr, &addrlen)) == ERROR) {
          perror("accept");
          continue;
        }
        if (fd_count < max_connections) {
          add_to_pfds(&pfds, newfd, &fd_count, &fd_size);

          if (cmdOps.option_v == 1) {
            char hostname[MAX_DATA_SIZE], servname[MAX_DATA_SIZE];
            if (getnameinfo((struct sockaddr *)&remoteaddr, 16, hostname, MAX_DATA_SIZE,
              servname, MAX_DATA_SIZE, NI_NAMEREQD) != SUCCESS) {
              perror("Localhost");
            } else {
              printf("Connection from localhost %s received!\n", servname);
            }
          }
        // If above max connections, put into queue
        } else {
          push(queue, &queue_count, &queue_size, newfd);
        }
      } else if (pfds[i].revents & POLLIN) {
        // Reset buf
        memset(buf, 0, MAX_DATA_SIZE - 1);
        // If not the listener, we're just a regular client
        const int nbytes = recv(pfds[i].fd, buf, sizeof buf, 0);

        const int sender_fd = pfds[i].fd;
        if (nbytes <= 0) {
          // Got error or connection closed by client
          if (nbytes != SUCCESS) {
            perror("recv");
            continue;
          }

          // Exit
          close(pfds[i].fd);

          del_from_pfds(pfds, i, &fd_count);

          // If not -k and fd_count is 2, exit
          if (cmdOps.option_k == 0 && fd_count == 2) {
            return 0;
          }
          if (queue_count > 0) {
            const int newfd = queue[0];
            add_to_pfds(&pfds, newfd, &fd_count, &fd_size);

            if (cmdOps.option_v == 1) {
              char hostname[MAX_DATA_SIZE], servname[MAX_DATA_SIZE];
              if (getnameinfo((struct sockaddr *)&remoteaddr, 16, hostname, MAX_DATA_SIZE,
                servname, MAX_DATA_SIZE, NI_NAMEREQD) != SUCCESS) {
                perror("Localhost");
              } else {
                printf("Connection from localhost %s received!\n", servname);
              }
            }
            pop(queue, &queue_count, queue_size);
          }
        } else {
          // We got some good data from a client
          printf("%s", buf);
          for(int j = 0; j < fd_count; j++) {
            // Send to everyone!
            const int dest_fd = pfds[j].fd;
            // Except the listener, stdin, and ourselves
            if (dest_fd != listener_fd && dest_fd != STDIN_FD && dest_fd != sender_fd) {
              if (send(dest_fd, buf, nbytes, 0) == ERROR) {
                perror("send");
              }
            }
          }
        }
      }
    }
  }
  return SUCCESS;
}

int client(struct commandOptions cmdOps) {
  char buf[MAX_DATA_SIZE];    // Buffer for data

  // Start off with room for 2 connections
  int fd_count = 0;
  int fd_size = 2;
  struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

  const int listener_fd = get_listener_socket(cmdOps, CLIENT);

  if (listener_fd == ERROR) {
    return ERROR;
  }

  // Add the listener to set
  pfds[0].fd = listener_fd;
  pfds[0].events = POLLIN; // Report ready to read on incoming connection

  pfds[1].fd = STDIN_FD;
  pfds[1].events = POLLIN;

  fd_count = 2; // For the listener

  if (cmdOps.option_v == 1) {
    printf("Connection to %s %d port [tcp/scp-config] succeeded!\n", cmdOps.hostname, cmdOps.port);
  }

  while(1) {
    if (poll(pfds, fd_count, cmdOps.timeout * 1000) == ERROR) {
      perror("poll");
      return ERROR;
    }
    
    // Run through the existing connections looking for data to read
    for(int i = 0; i < fd_count; i++) {
      // Check if someone's ready to read
      // stdin
      if ((pfds[i].revents & POLLIN) && pfds[i].fd == STDIN_FD) {
        memset(buf, 0, MAX_DATA_SIZE - 1);
        int nbytes;
        if ((nbytes = read(pfds[i].fd, buf, sizeof buf)) == ERROR) {
          perror("read");
          return ERROR;
        }
        for(int j = 0; j < fd_count; j++) {
          // Send to everyone!
          const int dest_fd = pfds[j].fd;
          // Except stdin
          if (dest_fd != STDIN_FD) {
            if (send(dest_fd, buf, nbytes, 0) == ERROR) {
              perror("send");
              return ERROR;
            }
          }
        }
      } else if ((pfds[i].revents & POLLIN)) {
        // Reset buf
        memset(buf, 0, MAX_DATA_SIZE - 1);
        int nbytes;
        if ((nbytes = recv(pfds[i].fd, buf, sizeof buf, 0)) > 0) {
          printf("%s", buf);
        } else if (nbytes != 0) {
          perror("recv");
          return ERROR;
        }
      }
    }
  }

  return SUCCESS;
}

int main(int argc, char **argv) {
  struct commandOptions cmdOps;
  const int retVal = parseOptions(argc, argv, &cmdOps);
  
  if (retVal != PARSE_OK) {
    exit(1);
  }

  if (cmdOps.option_l == 1) {
    server(cmdOps);
  } else {
    client(cmdOps);
  }
}
