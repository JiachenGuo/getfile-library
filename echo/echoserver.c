#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <getopt.h>

#if 0
/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/* 
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

#define BUFSIZE 2000
#define MSG_SIZE 16
#define SOCKET_ERROR -1

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoserver [options]\n"                                                    \
"options:\n"                                                                  \
"  -p                  Port (Default: 8803)\n"                                \
"  -m                  Maximum pending connections (default: 8)\n"            \
"  -h                  Show this help message\n"                              \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"maxnpending",   required_argument,      NULL,           'm'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};


void grace_exit(int sfd) {
    close(sfd);
}

int main(int argc, char **argv) {
  int option_char;
  int portno = 8803; /* port to listen on */
  int maxnpending = 8;
  
  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:m:h", gLongOptions, NULL)) != -1) {
   switch (option_char) {
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
      case 'p': // listen-port
        portno = atoi(optarg);
        break;                                        
      case 'm': // server
        maxnpending = atoi(optarg);
        break; 
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
    }
  }

    if (maxnpending < 1) {
        fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__, maxnpending);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

  /* Socket Code Here */
    int netsockfd,sockfd;  /* handle to socket */
    struct sockaddr_in server_addr, client_addr; /* Internet socket server_addr stuct */
    socklen_t socklen;
    char buffer[MSG_SIZE];
    int reuse = 1;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd == SOCKET_ERROR) {
      //exit(1);
  }

  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(portno);
  server_addr.sin_family = AF_INET;

  // those all too hacky! no failure handling!
  if (setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *) &reuse, sizeof(reuse)) == SOCKET_ERROR) {
      //close(sockfd);
      //exit(1);
  }

  /* bind to port */
  if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
      //exit(1);
  }

  /* socket port num */
  if (listen(sockfd, maxnpending) == SOCKET_ERROR) {
      //exit(1);
  }

    socklen = sizeof(client_addr);

    while(1) {
        netsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &socklen);

        bzero(buffer, MSG_SIZE);

        read(netsockfd, buffer, MSG_SIZE);
        printf("%s", buffer); // new line does matter
        write(netsockfd, buffer, MSG_SIZE);

        close(netsockfd);
    }

    return 0;
}
