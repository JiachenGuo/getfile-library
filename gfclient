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
// TODO(mwang) clean up student header
#include <assert.h>

#include "gfclient.h"
#include "gfclient-student.h"

#define MAX_REQUESTS 8
#define BUFSIZE 64000
#define HEADERSIZE 64000
#define FILEPATHSIZE 64000
#define SCHEMESIZE 64000
#define METHODSIZE 64000

struct gfcrequest_t {
    // socket metadata
    char *server;
    unsigned short port;
    int sockfd;
    
    // gf protocol metadata
    char *path;
    char *request;

    // gf received metadata
    size_t bytes_received;
    size_t filelen;
    gfstatus_t status;

    // callback funcs
    void (*headerfunc)(void *, size_t, void *);
    void *headerarg;
    void (*writefunc)(void *, size_t, void *);
    void *writearg;
};

const char *request_format = "%s %s %s %s";

const char *response_format = "%s %s %zd\r\n\r\n%n";

void gfc_cleanup(gfcrequest_t *gfr){
    assert(gfr != NULL);
    free(gfr->request);
    free(gfr);
}

gfcrequest_t *gfc_create(){
    gfcrequest_t *request = malloc(sizeof(gfcrequest_t));
    bzero(request, sizeof(gfcrequest_t));

    return request;
}

size_t gfc_get_bytesreceived(gfcrequest_t *gfr){
    assert(gfr != NULL);
    return gfr->bytes_received;
}

size_t gfc_get_filelen(gfcrequest_t *gfr){
    assert(gfr != NULL);
    return gfr->filelen;
}

gfstatus_t gfc_get_status(gfcrequest_t *gfr){
    assert(gfr != NULL);
    return gfr->status;
}

void gfc_global_init(){
}

void gfc_global_cleanup(){
}

int gfc_perform(gfcrequest_t *gfr){
    struct sockaddr_in serv_addr;
    struct hostent *server;

    gfr->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server = gethostbyname(gfr->server);

    // establish connection
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(gfr->port);

    if (connect(gfr->sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("failed to connect to server\n");
        gfr->status = GF_ERROR;
        return -1;
    }

    // build request
    gfr->request = (char *) malloc(BUFSIZE * sizeof(char));
    sprintf(gfr->request, request_format, gf_schema, gf_method, gfr->path, gf_eof);

    // send request
    ssize_t request_size = strlen(gfr->request);
    ssize_t bytes_sent = 0;

    while (bytes_sent < request_size) {
        printf("in loop\n");
        ssize_t sent = send(gfr->sockfd, gfr->request + bytes_sent, request_size - bytes_sent, 0);
        if (sent < 0) {
            printf("failed to send request to server\n");
            break;
        }
        bytes_sent += sent;
        printf("request size %zd, sent %zd\n", request_size, bytes_sent);
    }
    shutdown(gfr->sockfd, SHUT_WR);

    printf("waiting for response\n");
    // response handling
    int parse_header_res = parse_header(gfr);
    if (parse_header_res < 0) {
        printf("invalid header\n");
        return -1;
    }

    // finish the remaining part
    int parse_rc_res = parse_remaining_content(gfr);
    if (parse_rc_res < 0) {
        printf("invalid content\n");
        return -1;
    }

    printf("ok %d, the res is %d\n", GF_OK, gfc_get_status(gfr));

    close(gfr->sockfd);
    return 0;
}

int parse_remaining_content(gfcrequest_t *gfr) {
    printf("parse remaining content\n");
    char buffer[HEADERSIZE];

    while(gfr->status == GF_OK && gfr->bytes_received < gfr->filelen) {
        bzero(buffer, HEADERSIZE);
        size_t resp_size = recv(gfr->sockfd, buffer, HEADERSIZE, 0);
        printf("remaining content resp size %zd\n", resp_size);

        if (resp_size == 0) {
            printf("file not completely received\n");
            return -1;
        }

        if (resp_size < 0) {
            printf("broken socket on remaining content\n");
            return -1;
        }

        gfr->writefunc(&buffer, resp_size, gfr->writearg);
        gfr->bytes_received += resp_size;
    }

    return 0;
}

int parse_header(gfcrequest_t *gfr) {
    char buffer[HEADERSIZE];
    bzero(buffer, HEADERSIZE);

    // assume a large chunk capable of reading all header fields
    ssize_t response_size;
    response_size = recv(gfr->sockfd, buffer, HEADERSIZE, 0);

    if (response_size == 0) {
        gfr->status = GF_INVALID;
        return -1;
    }

    if (response_size < 0) {
        gfr->status = GF_ERROR;
        return -1;
    }

    char schema[SCHEMESIZE];
    char status[SCHEMESIZE];
    size_t file_len = 0;
    int pos_file_starts = 0;

    printf("in the buffer:%s\n", buffer);
    if (sscanf(buffer, response_format, schema, status, &file_len, &pos_file_starts) == EOF) {
        printf("scaning header failed\n");
        gfr->status = GF_INVALID;
        return -1;
    }

    int is_valid_status_int = is_valid(schema, status, gfr);
    if (is_valid_status_int < 0) {
        printf("invalid header\n");
        gfr->status = GF_INVALID;
        return -1;
    }

    gfr->filelen = file_len;

    // if has content in this batch
    if (is_valid_status_int == 0 && pos_file_starts < response_size) {
        size_t bytes_to_write = response_size - pos_file_starts;
        gfr->writefunc(buffer + pos_file_starts, bytes_to_write, gfr->writearg);
        gfr->bytes_received += bytes_to_write;
    }

    return 0;
}

int is_valid(char *schema, char *status, gfcrequest_t *gfr) {
    if (schema == NULL || status == NULL) {
        return -1;
    }

    printf("header: %s, status: %s\n", schema, status);

    if (strcmp(schema, gf_schema) != 0) {
        return -1;
    }

    if (strcmp(status, gf_ok_string) == 0) {
        gfr->status = GF_OK;
        return 0;
    }

    if (strcmp(status, gf_file_not_found_string) == 0) {
        gfr->status = GF_FILE_NOT_FOUND;
        return 1;
    }

    if (strcmp(status, gf_error_string) == 0) {
        gfr->status = GF_ERROR;
        return 2;
    }

    return -1;
}

void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg){
    assert(gfr != NULL);
    gfr->headerarg = headerarg;
}

void gfc_set_headerfunc(gfcrequest_t *gfr, void (*headerfunc)(void*, size_t, void *)){
    assert(gfr != NULL);
    assert(headerfunc != NULL);
    gfr->headerfunc = headerfunc;
}

void gfc_set_path(gfcrequest_t *gfr, char* path){
    assert(path != NULL);
    gfr->path = path;
}

void gfc_set_port(gfcrequest_t *gfr, unsigned short port){
    gfr->port = port;
}

void gfc_set_server(gfcrequest_t *gfr, char* server){
    assert(server != NULL);
    gfr->server = server;
}

void gfc_set_writearg(gfcrequest_t *gfr, void *writearg){
    assert(gfr != NULL);
    gfr->writearg = writearg;
}

void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *)){
    assert(gfr != NULL);
    assert(writefunc != NULL);
    gfr->writefunc = writefunc;
}

char* gfc_strstatus(gfstatus_t status){
    if (status == GF_OK) {
        return (char *) gf_ok_string;
    } else if (status == GF_FILE_NOT_FOUND) {
        return (char *) gf_file_not_found_string;
    } else {
        return (char *) gf_error_string;
    }
}
