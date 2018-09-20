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

/* Be prepared accept a response of this length */
#define BUFSIZE 2000
#define MSG_SIZE 16
#define SOCKET_ERROR -1

#define USAGE                                                                       \
    "usage:\n"                                                                      \
    "  echoclient [options]\n"                                                      \
    "options:\n"                                                                    \
    "  -s                  Server (Default: localhost)\n"                           \
    "  -p                  Port (Default: 8803)\n"                                  \
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
    unsigned short portno = 8803;
    char *message = "hello world.";

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:m:h", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 's': // server
            hostname = optarg;
            break;
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 'm': // message
            message = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        }
    }

    if (NULL == message)
    {
        fprintf(stderr, "%s @ %d: invalid message\n", __FILE__, __LINE__);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535))
    {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    if (NULL == hostname)
    {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
	char buffer[MSG_SIZE];

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == SOCKET_ERROR) {
        exit(0);
	}

    server = gethostbyname(hostname);
    if (server == NULL) {
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);

    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        exit(0);
	}

    n = write(sockfd, message, strlen(message));
    if (n == SOCKET_ERROR) {
        exit(0);
	}
    bzero(buffer, MSG_SIZE);
    n = read(sockfd, buffer, MSG_SIZE);
    if (n == SOCKET_ERROR) {
        exit(0);
    }
    printf("%s", buffer);

    close(sockfd);
    return 0;
}
