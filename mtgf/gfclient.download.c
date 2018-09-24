#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <getopt.h>
#include <pthread.h>

#include "gfclient.h"
#include "workload.h"

#include "gfclient-student.h"
#include "steque.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -h                  Show this help message\n"                              \
"  -n [num_requests]   Requests download per thread (Default: 2)\n"           \
"  -p [server_port]    Server port (Default: 8803)\n"                         \
"  -s [server_addr]    Server address (Default: 0.0.0.0)\n"                   \
"  -t [nthreads]       Number of threads (Default 2)\n"                       \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
        {"help",          no_argument,       NULL, 'h'},
        {"nthreads",      required_argument, NULL, 't'},
        {"nrequests",     required_argument, NULL, 'n'},
        {"server",        required_argument, NULL, 's'},
        {"port",          required_argument, NULL, 'p'},
        {"workload-path", required_argument, NULL, 'w'},
        {NULL, 0,                            NULL, 0}
};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t cond_worker = PTHREAD_COND_INITIALIZER;

static steque_t *queue;

static char *server;

static unsigned short port;

static int nrequests;


static void Usage() {
    fprintf(stdout, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path) {
    static int counter = 0;

    sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE *openFile(char *path) {
    char *cur, *prev;
    FILE *ans;

    /* Make the directory if it isn't there */
    prev = path;
    while (NULL != (cur = strchr(prev + 1, '/'))) {
        *cur = '\0';

        if (0 > mkdir(&path[0], S_IRWXU)) {
            if (errno != EEXIST) {
                perror("Unable to create directory");
                exit(EXIT_FAILURE);
            }
        }

        *cur = '/';
        prev = cur;
    }

    if (NULL == (ans = fopen(&path[0], "w"))) {
        perror("Unable to open file");
        exit(EXIT_FAILURE);
    }

    return ans;
}

/* Callbacks ========================================================= */
static void writecb(void *data, size_t data_len, void *arg) {
    FILE *file = (FILE *) arg;

    fwrite(data, 1, data_len, file);
}

void *process(void *nthread) {
    gfcrequest_t *gfr = NULL;
    FILE *file = NULL;
    int returncode = 0;
    char *req_path = NULL;
    char local_path[512];

    for (int i = 0; i < nrequests; i++) {
        pthread_mutex_lock(&mutex);

        while (steque_isempty(queue)) {
            pthread_cond_wait(&cond_worker, &mutex);
        }

        req_path = steque_pop(queue);

        pthread_mutex_unlock(&mutex);

        localPath(req_path, local_path);

        file = openFile(local_path);

        gfr = gfc_create();
        gfc_set_server(gfr, server);
        gfc_set_path(gfr, req_path);
        gfc_set_port(gfr, port);
        gfc_set_writefunc(gfr, writecb);
        gfc_set_writearg(gfr, file);

        fprintf(stdout, "Requesting %s%s\n", server, req_path);

        if (0 > (returncode = gfc_perform(gfr))) {
            fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
            fclose(file);
            if (0 > unlink(local_path))
                fprintf(stderr, "unlink failed on %s\n", local_path);
        } else {
            fclose(file);
        }

        if (gfc_get_status(gfr) != GF_OK) {
            if (0 > unlink(local_path))
                fprintf(stderr, "unlink failed on %s\n", local_path);
        }

        fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(gfr)));
        fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr));

        gfc_cleanup(gfr);
    }
    return 0;
}

/* Main ========================================================= */
int main(int argc, char **argv) {
/* COMMAND LINE OPTIONS ============================================= */
    server = "localhost";
    port = 8803;
    nrequests = 2;

    char *workload_path = "workload.txt";

    int i = 0;
    int option_char = 0;
    int nthreads = 2;
//    int returncode = 0;
//    gfcrequest_t *gfr = NULL;
//    FILE *file = NULL;
    char *req_path = NULL;
//    char local_path[512];

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "hn:p:s:t:w:", gLongOptions, NULL)) != -1) {
        switch (option_char) {
            case 'h': // help
                Usage();
                exit(0);
                break;
            case 'n': // nrequests
                nrequests = atoi(optarg);
                break;
            case 'p': // port
                port = atoi(optarg);
                break;
            case 's': // server
                server = optarg;
                break;
            case 't': // nthreads
                nthreads = atoi(optarg);
                break;
            case 'w': // workload-path
                workload_path = optarg;
                break;
            default:
                Usage();
                exit(1);
        }
    }

    if (EXIT_SUCCESS != workload_init(workload_path)) {
        fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
        exit(EXIT_FAILURE);
    }

    gfc_global_init();

    //
    queue = malloc(sizeof(steque_t));
    pthread_t *workers = malloc(nthreads * sizeof(pthread_t));
    int *thread_ids = malloc(nthreads * sizeof(int));

    steque_init(queue);

    for (i = 0; i < nthreads; i++) {
        thread_ids[i] = i;
        pthread_create(&workers[i], NULL, process, &thread_ids[i]);
    }

    /*Making the requests...*/
    for (i = 0; i < nrequests * nthreads; i++) {
        req_path = workload_get_path();

        if (strlen(req_path) > 256) {
            fprintf(stderr, "Request path exceeded maximum of 256 characters\n.");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&mutex);

        steque_enqueue(queue, req_path);

        pthread_cond_signal(&cond_worker);

        pthread_mutex_unlock(&mutex);
    }

    for (i = 0; i < nthreads; i++) {
        if (pthread_join(workers[i], NULL) < 0) {
            printf("joining thread failed %d\n", i);
        }
    }

    /* old code frag
        localPath(req_path, local_path);

        file = openFile(local_path);

        gfr = gfc_create();
        gfc_set_server(gfr, server);
        gfc_set_path(gfr, req_path);
        gfc_set_port(gfr, port);
        gfc_set_writefunc(gfr, writecb);
        gfc_set_writearg(gfr, file);

        fprintf(stdout, "Requesting %s%s\n", server, req_path);

        if (0 > (returncode = gfc_perform(gfr))) {
            fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
            fclose(file);
            if (0 > unlink(local_path))
                fprintf(stderr, "unlink failed on %s\n", local_path);
        } else {
            fclose(file);
        }

        if (gfc_get_status(gfr) != GF_OK) {
            if (0 > unlink(local_path))
                fprintf(stderr, "unlink failed on %s\n", local_path);
        }

        fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(gfr)));
        fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr));

        gfc_cleanup(gfr);

    }
     */

    gfc_global_cleanup();
    steque_destroy(queue);
    free(queue);
    free(workers);
    free(thread_ids);

    return 0;
}  
