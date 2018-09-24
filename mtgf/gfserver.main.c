#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>

#include "gfserver.h"
#include "content.h"
#include "steque.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  gfserver_main [options]\n"                                                 \
"options:\n"                                                                  \
"  -t [nthreads]       Number of threads (Default: 3)\n"                      \
"  -p [listen_port]    Listen port (Default: 8803)\n"                         \
"  -m [content_file]   Content file mapping keys to content files\n"          \
"  -h                  Show this help message.\n"                             \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
        {"port",     required_argument, NULL, 'p'},
        {"nthreads", required_argument, NULL, 't'},
        {"content",  required_argument, NULL, 'm'},
        {"help",     no_argument,       NULL, 'h'},
        {NULL, 0,                       NULL, 0}
};


extern ssize_t handler_get(gfcontext_t *ctx, char *path, void *arg);
extern ssize_t transfer_handler(gfcontext_t *ctx, char *path);

static steque_t *queue;

static pthread_t *workers;

static int nthreads;

static gfserver_t *gfs;

pthread_mutex_t mutext = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_worker = PTHREAD_COND_INITIALIZER;

typedef struct queue_ctx {
    gfcontext_t *ctx;
    char *file_path;
} queue_ctx;

static void _sig_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        printf("im here\n");
        exit(signo);
    }
}

void cleanup() {
    printf("in clean up\n");

    for (int i = 0; i < nthreads; i++) {
        if (pthread_join(workers[i], NULL) < 0) {
            printf("failed to join pthread\n");
        }
    }

    steque_destroy(queue);
    free(queue);
    free(workers);
    free(gfs);
    content_destroy();
}

void enqueue(gfcontext_t *ctx, char *file_path) {
    queue_ctx *q_ctx = malloc(sizeof(queue_ctx));
    q_ctx->ctx = ctx;
    q_ctx->file_path = file_path;
    steque_enqueue(queue, q_ctx);
}

void *process(void *args) {
    while (1) {
        pthread_mutex_lock(&mutext);

        while (steque_isempty(queue)) {
            pthread_cond_wait(&cond_worker, &mutext);
        }

        queue_ctx *q_ctx = steque_pop(queue);

        pthread_mutex_unlock(&mutext);

        transfer_handler(q_ctx->ctx, q_ctx->file_path);

        if (q_ctx->ctx != NULL) {
            gfs_abort(q_ctx->ctx);
        }

        free(q_ctx->file_path);
        free(q_ctx);
    }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
    int option_char = 0;
    unsigned short port = 8803;
    char *content_map = "content.txt";

    gfs = NULL;
    nthreads = 3;

    if (signal(SIGINT, _sig_handler) == SIG_ERR) {
        cleanup();
        exit(0);
    }

    if (signal(SIGTERM, _sig_handler) == SIG_ERR) {
        cleanup();
        exit(0);
    }

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "m:p:t:h", gLongOptions, NULL)) != -1) {
        switch (option_char) {
            default:
                fprintf(stderr, "%s", USAGE);
                exit(1);
            case 'p': // listen-port
                port = atoi(optarg);
                break;
            case 't': // nthreads
                nthreads = atoi(optarg);
                break;
            case 'h': // help
                fprintf(stdout, "%s", USAGE);
                exit(0);
                break;
            case 'm': // file-path
                content_map = optarg;
                break;
        }
    }

    /* not useful, but it ensures the initial code builds without warnings */
    if (nthreads < 1) {
        nthreads = 1;
    }

    content_init(content_map);

    /*Initializing server*/
    gfs = gfserver_create();

    /*Setting options*/
    gfserver_set_port(gfs, port);
    gfserver_set_maxpending(gfs, 64);
    gfserver_set_handler(gfs, handler_get);
    gfserver_set_handlerarg(gfs, NULL);

    queue = malloc(sizeof(steque_t));
    steque_init(queue);

    workers = malloc(sizeof(pthread_t));

    for (int i = 0; i < nthreads; i++) {
        if (pthread_create(&workers[i], NULL, process, NULL) != 0) {
            printf("failed to create worker %d\n", i);
            return -1;
        }
    }

    /*Loops forever*/
    printf("start serving\n");
    gfserver_serve(gfs);

    printf("stop serving\n");
    cleanup();
}
