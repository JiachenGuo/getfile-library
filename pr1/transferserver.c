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

#define BUFSIZE 1033

#define USAGE                                                \
    "usage:\n"                                               \
    "  transferserver [options]\n"                           \
    "options:\n"                                             \
    "  -f                  Filename (Default: 6200.txt)\n" \
    "  -h                  Show this help message\n"         \
    "  -p                  Port (Default: 12041)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"filename", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int option_char;
    int portno = 12041;             /* port to listen on */
    char *filename = "6200.txt"; /* file to transfer */

    setbuf(stdout, NULL); // disable buffering

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:hf:x", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        case 'f': // listen-port
            filename = optarg;
            break;
        }
    }


    if ((portno < 1025) || (portno > 65535))
    {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    
    if (NULL == filename)
    {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */
	int netsockfd, sockfd;
	int reuse_option = 1; // socket reuse option 1 == ON
	int maxnpending = 5;
	struct sockaddr_in serv_addr, cli_addr;
	//socklen_t cli_len;
	char buffer[BUFSIZE];
	FILE *fp;
	int total_size;
	// create and setup socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Failed to create socket\n");
		exit(1);
	}
	setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&reuse_option, sizeof(reuse_option));
	//configure server socket
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_addr.s_addr = INADDR_ANY; 
	serv_addr.sin_port = htons(portno);
	// bind the server Socket
	bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	listen(sockfd, maxnpending);

	while(1) {
		netsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &(sizeof(cli_addr)));
		total_size = 0;
		fp = fopen(filename, "r+");
		bzero(buffer, BUFSIZE);

		if (fp == NULL) {
			fprintf(stderr, "Failed to open file %s.\n", filename);
			close(netsockfd);
			exit(1);
		}
		bzero(buffer, BUFSIZE);
		int read_len;
		while ((read_len = fread(buffer, sizeof(char), BUFSIZE, file)) > 0) {
			if (send(netsockfd, buffer, read_len, 0) < 0) {
				fprintf(stderr, "File failed to send.\n");
				fclose(fp);
				close(netsockfd);
				exit(1);
			}
			total_size += read_len;
		}

		printf("Total bytes sent %d\n", total_size);

		fclose(fp);
		close(netsockfd);
	}

	return 0;

}
