#include "commonProto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


// This is the start of the code to parse the command line options. It should be
// fairly complete but hasn't been extensively tested.

int parseOptions(int argc, char * argv[], struct commandOptions * co) {
  // First set the command options structure to known values
  co->option_k = 0;
  co->option_l = 0;
  co->option_v = 0;
  co->option_r = 0;
  co->option_p = 0;
  co->source_port = 0;
  co->timeout = -1;
  co->hostname = NULL;
  co->port = 0;
  
  int lastTwo = 0;
  int indexOfLastTwo = -1;
  
  // iterate over each argument and classify it and
  // store information in the co structure;
  
  if (argc <= 1) {
    usage(argv[0]);
    return PARSE_ERROR;
  }

  for (int i = 1; i < argc; i++) {
    // Check for the various options
    if (strcmp(argv[i], K_OPTION) == 0) {
      co->option_k = 1;
    } else if (strcmp(argv[i], L_OPTION) == 0) {
      co->option_l = 1;
    } else if (strcmp(argv[i], V_OPTION) == 0) {
      co->option_v = 1;
    } else if (strcmp(argv[i], R_OPTION) == 0) {
      co->option_r = 1;
    } else if (strcmp(argv[i], P_OPTION) == 0) {
      // got a port match, check next argument for port number
      i++;
      if (i >= argc) {
        // not enough arguments
        printf("nc: option requires an argument -- 'p'\n");
        usage(argv[0]);
        return PARSE_ERROR;
      } else { // extract port number
        // See man page for strtoul() as to why
        // we check for errors by examining errno
        errno = 0;
        co->source_port = strtoul(argv[i], NULL, 10);
        if (errno != 0) {
          return PARSE_ERROR;
        } else {
          co->option_p = 1;
        }
      }
    } else if ((strcmp(argv[i], W_OPTION) == 0) && (!lastTwo)) {
      // got a W match, check next argument for timeout value
      i++;
      if (i >= argc) {
        // not enough arguments
        printf("nc: option requires an argument -- 'w'\n");
        usage(argv[0]);
        return PARSE_ERROR;
      } else { // extract timeout value
        // See man page for strtoul() as to why
        // we check for errors by examining errno, see err
        errno = 0;
        co->timeout = strtoul(argv[i], NULL, 10);
        if (errno != 0) {
          printf("nc: timeout invalid: %s\n", argv[i]);
          return PARSE_ERROR;
        }
      }
      // Things are tricker now as this must be either the hostname or port number
      // and if there are more parameters on the line then this is a bug
    } else if (lastTwo == 0) { // hostname
      errno = 0;
      indexOfLastTwo = i;
      co->port = strtoul(argv[i], NULL, 10);
      if (errno != 0) {
        return PARSE_ERROR;
      }
      ++lastTwo;
    } else if (lastTwo == 1) { // port
      co->hostname = argv[indexOfLastTwo];
      errno = 0;
      co->port = strtoul(argv[i], NULL, 10);
      if (errno != 0) {
        return PARSE_ERROR;
      }
      ++lastTwo;
    } else { // TOO many parameters
      printf("nc: invalid option -- '%s'\n", argv[i]);
      usage(argv[0]);
      return PARSE_TOOMANY_ARGS;
    }
  }

  if (co->option_k == 1 && co->option_l == 0) { // If -k, -l must also be set
    printf("nc: must use -l with -k\n");
    return PARSE_ERROR;
  } else if (co->option_l == 1 && co->option_p == 1) { // -l and -p cannot be used together
    return PARSE_ERROR;
  } else if (co->option_r == 1 && co->option_l == 0) { // If -r, -l must also be set
    printf("nc: must use -r with -l\n");
    return PARSE_ERROR;
  } else if (co->port <= RESERVED_PORT) {
    printf("nc: Permission denied\n");
    return PARSE_ERROR;
  }

  // If -l, timeout is ignored
  if (co->option_l == 1) {
    co->timeout = -1;
  }

  return PARSE_OK;
}
