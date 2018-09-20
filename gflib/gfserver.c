#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
// TODO clean up student header
#include <assert.h>

#include "gfserver.h"
#include "gfserver-student.h"

#define BUFSIZE 64000
#define HEADERSIZE 64000
#define FILEPATHSIZE 64000
#define SCHEMESIZE 64000
#define METHODSIZE 64000

/*
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */

struct gfserver_t {
    // server metadata
    unsigned short port;
    int max_npending;
    int sockfd;

    // return data
    gfstatus_t status;
    size_t filelen;
    gfcontext_t *ctx;

    ssize_t (*handler)(gfcontext_t *, char *, void *);
    void *handlerarg;
};

struct gfcontext_t {
    // metadata
    int sockfd;

};

static const int reusable = 1;

static const char *success_resp_header_format = "%s %s %zd %s";

static const char *fail_resp_header_format = "%s %s %s";

static const char *header_format = "%s %s %s\r\n\r\n%s";

void gfs_abort(gfcontext_t *ctx){
    if (ctx == NULL) {
        printf("null ctx to abort, will return\n");
        return;
    }

    printf("send abort request to client\n");
    gfs_sendheader(ctx, GF_ERROR, 0);

    printf("sent, trying to close connection\n");
    shutdown(ctx->sockfd, SHUT_WR);
    close(ctx->sockfd);
}

gfserver_t* gfserver_create(){
    gfserver_t *gfserver = malloc(sizeof(gfserver_t));
    bzero(gfserver, sizeof(gfserver_t));

    gfserver->ctx = malloc(sizeof(gfcontext_t));
    return gfserver;
}

ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len){
    assert(ctx != NULL);
    assert(data != NULL);

    ssize_t bytes_sent = sendcontent(ctx->sockfd, data, len);
    if (bytes_sent != len) {
        printf("bytes sent %zi not equal to total %zu\n", bytes_sent, len);
    }

    return bytes_sent;
}

ssize_t sendcontent(int sockfd, char *data, size_t len) {
    // pointer checked by caller
    ssize_t bytes_sent = 0;

    while (bytes_sent < len) {
        ssize_t current_sent = send(sockfd, &data[bytes_sent], len - bytes_sent, 0);
        if (current_sent < 0) {
            printf("error on send content\n");
            break;
        }

        bytes_sent += current_sent;
    }

    return bytes_sent;
}

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len) {
    assert(ctx != NULL);

    char header[BUFSIZE];

    printf("status %d\n", (int) status);

    if (status == GF_OK) {
        sprintf(header, success_resp_header_format, gf_schema, gf_ok_string, file_len, gf_eof);
    } else if (status == GF_FILE_NOT_FOUND) {
        sprintf(header, fail_resp_header_format, gf_schema, gf_file_not_found_string, gf_eof);
    } else {
        sprintf(header, fail_resp_header_format, gf_schema, gf_error_string, gf_eof);
    }

    printf("sending header from gfs_sendheader %s\n", header);

    ssize_t header_len = strlen(header);
    if (sendcontent(ctx->sockfd, header, header_len) < header_len) {
        printf("failed to send header to client");
        return -1;
    }

    return header_len;
}

int process_request(gfserver_t *gfs) {
    char buffer[BUFSIZE];
    char header[HEADERSIZE];
    size_t request_size, bytes_received;

    bzero(buffer, BUFSIZE);
    bzero(header, HEADERSIZE);
    request_size = 0;
    bytes_received = 0;

    printf("start reading from socket\n");
    // read header from socket
    while (bytes_received < HEADERSIZE) {
        printf("in loop\n");
        request_size = recv(gfs->ctx->sockfd, buffer, BUFSIZE, 0);
        printf("request size %zd\n", request_size);
        if (request_size < 0) {
            printf("error while reading from socket, will send error\n");
            return -1;
        }
        if (request_size == 0) {
            printf("no more bytes coming from socket\n");
            break;
        }

        if ((request_size + bytes_received) <= HEADERSIZE) {
            printf("less than lim\n");
            memcpy(header + bytes_received, buffer, request_size);
            bytes_received += request_size;
        } else {
            // for now
            printf("reached  lim, will quit next loop\n");
            memcpy(header + bytes_received, buffer, HEADERSIZE - bytes_received);
            bytes_received = HEADERSIZE;
        }

        // handle eof
        if (strstr(header, gf_eof) != NULL) {
            printf("full header received\n");
            break;
        }
    }

    printf("parsing header\n");

    char file_path[FILEPATHSIZE] = "";
    char schema[SCHEMESIZE] = "";
    char method[METHODSIZE] = "";

    if (sscanf(header, header_format, schema, method, file_path) != EOF) {
        int is_valid = is_valid_request(schema, method, file_path);
        if (is_valid == 0) {
            printf("valid request, will call handler\n");
            gfs->handler(gfs->ctx, file_path, gfs->handlerarg);
        } else {
            printf("invalid request header %d\n", is_valid);
            gfs_sendheader(gfs->ctx, GF_FILE_NOT_FOUND, 0);
        }
    }

    return 0;
}

void gfserver_serve(gfserver_t *gfs){
    assert(gfs != NULL);

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;

    if ((gfs->sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Failed to create socket\n");
    }

    if ((setsockopt(gfs->sockfd, SOL_SOCKET, (SO_REUSEADDR | SO_REUSEPORT), &reusable, sizeof(reusable))) < 0) {
        printf("Failed to set socket\n");
    }

    // set up server addr
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(gfs->port);

    // bind sock
    bind(gfs->sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    listen(gfs->sockfd, gfs->max_npending);

    while (1) {
        bzero(gfs->ctx, sizeof(gfcontext_t));
        gfs->ctx->sockfd = accept(gfs->sockfd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (gfs->ctx->sockfd < 0) {
            printf("failed to create client socket\n");
            continue;
        }

        int proc_res = process_request(gfs);
        if (proc_res < 0) {
            printf("server will send error response to server\n");
            gfs_sendheader(gfs->ctx, GF_ERROR, 0);
        }

        int close_sig = close(gfs->ctx->sockfd);
        if (close_sig < 0) {
            printf("failed to close socket\n");
        }
    }
}

int is_valid_request(char *schema, char *method, char *path) {
    if (schema == NULL || method == NULL || path == NULL) {
        return -1;
    }

    // if three are equal, should return 0
    return strcmp(schema, gf_schema) + strcmp(method, gf_method) + strncmp(path, "/", 1);
}

void gfserver_set_handlerarg(gfserver_t *gfs, void* arg){
    assert(gfs != NULL);
    gfs->handlerarg = arg;
}

void gfserver_set_handler(gfserver_t *gfs, ssize_t (*handler)(gfcontext_t *, char *, void*)){
    assert(gfs != NULL);
    assert(handler != NULL);
    gfs->handler = handler;
}

void gfserver_set_maxpending(gfserver_t *gfs, int max_npending){
    assert(gfs != NULL);
    gfs->max_npending = max_npending;
}

void gfserver_set_port(gfserver_t *gfs, unsigned short port){
    assert(gfs != NULL);
    gfs->port = port;
}
