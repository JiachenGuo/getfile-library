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
/* 
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
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

#define BUFSIZE 2

#define USAGE                                                \
    "usage:\n"                                               \
    "  transferserver [options]\n"                           \
    "options:\n"                                             \
    "  -f                  Filename (Default: cs8803.txt)\n" \
    "  -h                  Show this help message\n"         \
    "  -p                  Port (Default: 8803)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"filename", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int option_char;
    int portno = 8803;             /* port to listen on */
    char *filename = "cs8803.txt"; /* file to transfer */

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:hf:", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        case 'f': // listen-port
            filename = optarg;
            break;
        }
    }

    if (NULL == filename)
    {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535))
    {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    /* Socket Code Here */
    int REUSABLE = 1;

    int netsockfd, sockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len;

    char buffer[BUFSIZE];
    FILE *file;
    int tot_bytes;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET; // internet address family
    serv_addr.sin_addr.s_addr = INADDR_ANY; // symbol address for server side
    serv_addr.sin_port = htons(portno);

    setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *) &REUSABLE, sizeof(REUSABLE));

    bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    listen(sockfd, 5);

    cli_len = sizeof(cli_addr);
    
    while(1) {
        netsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_len);
        tot_bytes = 0;

        file = fopen(filename, "r+");
        bzero(buffer, BUFSIZE);
        while(!feof(file)) {
            int read_len = fread(buffer, 1, BUFSIZE, file);
            if (read_len < 1) {
                continue;
            }

            int off = 0;
            while(off < read_len) {
                int sent = send(netsockfd, buffer + off, read_len - off, 0);
                off += sent;
                tot_bytes += sent;
            }
        }
        printf("bytes sent %d\n", tot_bytes);

        fclose(file);
        close(netsockfd);
    }

    return 0;
}
