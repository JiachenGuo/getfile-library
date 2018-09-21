#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <getopt.h>

/* Be prepared accept a response of this length */
#define BUFSIZE 1033
#define SOCKET_ERROR -1

#define USAGE                                                                       \
    "usage:\n"                                                                      \
    "  echoclient [options]\n"                                                      \
    "options:\n"                                                                    \
    "  -s                  Server (Default: localhost)\n"                           \
    "  -p                  Port (Default: 12041)\n"                                  \
    "  -m                  Message to send to server (Default: \"hello world.\")\n" \
    "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"message", required_argument, NULL, 'm'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int option_char = 0;
    char *hostname = "localhost";
    unsigned short portno = 12041;
    char *message = "Hello World!!";

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:m:hx", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
        case 's': // server
            hostname = optarg;
            break;
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 'm': // message
            message = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        }
    }

    setbuf(stdout, NULL); // disable buffering

    if ((portno < 1025) || (portno > 65535))
    {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    if (NULL == message)
    {
        fprintf(stderr, "%s @ %d: invalid message\n", __FILE__, __LINE__);
        exit(1);
    }

    if (NULL == hostname)
    {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */
    int hSocket, n;                 /* handle to socket */
    struct hostent* pHostInfo;   /* holds info about a machine */
    struct sockaddr_in Address;  /* Internet socket address stuct */
    char buffer[16];
    long nHostAddress;
    hSocket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(hSocket == SOCKET_ERROR)
    {
        exit(1);
    }
     /* get IP address from name */
    pHostInfo=gethostbyname(strHostName);
    if (pHostInfo == NULL) {
        exit(1);
    }
	bzero((char *)&Address, sizeof(Address));
    //memcpy(&nHostAddress,pHostInfo->h_addr,pHostInfo->h_length);
    Address.sin_family=AF_INET;
	bcopy((char *)pHostInfo->h_addr,
		(char *)&Address.sin_addr.s_addr,
		pHostInfo->h_length);
    Address.sin_port = htons(portno);

    if (connect(hSocket, (struct sockaddr *) &Address, sizeof(Address)) == SOCKET_ERROR) {
        exit(1);
	}
    n = write(hSocket, message, strlen(message));
    if (n == SOCKET_ERROR) {
        exit(1);
	}
	bzero(buffer, 16);
    n = read(hSocket, message, strlen(message));
    if (n == SOCKET_ERROR) {
        exit(1);
	}
    printf("%s", buffer);

    close(hSocket);
    return 0;

}
