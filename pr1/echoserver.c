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
#define SOCKET_ERROR -1
#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoserver [options]\n"                                                    \
"options:\n"                                                                  \
"  -p                  Port (Default: 12041)\n"                                \
"  -m                  Maximum pending connections (default: 1)\n"            \
"  -h                  Show this help message\n"                              \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"maxnpending",   required_argument,      NULL,           'm'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};


int main(int argc, char **argv) {
  int option_char;
  int portno = 12041; /* port to listen on */
  int maxnpending = 1;
  
  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:m:hx", gLongOptions, NULL)) != -1) {
   switch (option_char) {
      case 'p': // listen-port
        portno = atoi(optarg);
        break;                                        
      default:
        fprintf(stderr, "%s ", USAGE);
        exit(1);
      case 'm': // server
        maxnpending = atoi(optarg);
        break; 
      case 'h': // help
        fprintf(stdout, "%s ", USAGE);
        exit(0);
        break;
    }
  }

    setbuf(stdout, NULL); // disable buffering

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    if (maxnpending < 1) {
        fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__, maxnpending);
        exit(1);
    }


  /* Socket Code Here */
    int hSocket,hServerSocket;  /* handle to socket */
    struct sockaddr_in server_addr, client_addr; /* Internet socket server_addr stuct */
    char buffer[16];
    int nAddressSize=sizeof(struct sockaddr_in);

    hServerSocket=socket(AF_INET,SOCK_STREAM,0);
    if(hServerSocket == SOCKET_ERROR)
    {
        exit(1);
    }

    /* fill address struct */
    server_addr.sin_addr.s_addr=INADDR_ANY;
    server_addr.sin_port=htons(portno);
    server_addr.sin_family=AF_INET;

    /* bind to a port */
    if(bind(hServerSocket,(struct sockaddr*)&server_addr,sizeof(server_addr)) 
                        == SOCKET_ERROR)
    {
        exit(1);
    }

     /* establish listen queue */
    if(listen(hServerSocket,maxnpending) == SOCKET_ERROR)
    {
        exit(1);
    }
    
    for(;;)
    {
        
        /* get the connected socket */
        hSocket=accept(hServerSocket,(struct sockaddr*)&client_addr,(socklen_t *)&nAddressSize);

	   /* number returned by read() and write() is the number of bytes
        ** read or written, with -1 being that an error occured
        ** write what we received back to the server */
        
        /* read from socket into buffer */
        read(hSocket,buffer,16);
        printf("%s", buffer); 
        write(hSocket,buffer,16);
        close(hSocket);


    }
    return 0




}
