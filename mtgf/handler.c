#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "gfserver.h"
#include "content.h"

#include "gfserver-student.h"

#define BUFFER_SIZE 8803

extern pthread_mutex_t mutext;
extern pthread_cond_t cond_worker;
extern void enqueue(gfcontext_t *ctx, char *file_path);

ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg) {
	pthread_mutex_lock(&mutext);
	enqueue(ctx, path);
	pthread_cond_signal(&cond_worker);
	pthread_mutex_unlock(&mutext);

	return 0;
}

ssize_t transfer_handler(gfcontext_t *ctx, char *path) {
	int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	char buffer[BUFFER_SIZE];

	if( 0 > (fildes = content_get(path)))
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);

	/* Determine the file size */
	file_len = lseek(fildes, 0, SEEK_END);

	gfs_sendheader(ctx, GF_OK, file_len);

	/* Send the file in chunks */
	bytes_transferred = 0;
	while(bytes_transferred < file_len){
		read_len = pread(fildes, buffer, BUFFER_SIZE, bytes_transferred);
		if (read_len <= 0){
			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu\n", read_len, bytes_transferred, file_len );
			gfs_abort(ctx);
			return -1;
		}
		write_len = gfs_send(ctx, buffer, read_len);
		if (write_len != read_len){
			fprintf(stderr, "handle_with_file write error, %zd != %zd\n", write_len, read_len);
			gfs_abort(ctx);
			return -1;
		}
		bytes_transferred += write_len;
	}

	return bytes_transferred;
}
