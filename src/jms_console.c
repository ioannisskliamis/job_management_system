#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "messages.h"
#include "console.h"

int main(int argc, char* argv[]) {
	int readfd, writefd;
	char *read_pipe = NULL, *write_pipe = NULL, *operations_file = NULL;

	// Parse cmd line arguments
	for (int i = 1; i < argc; i++) {

		if ((strcmp(argv[i], "-w") == 0) && (i + 1 < argc)) {
			write_pipe = argv[i + 1];
			i++;		// Skip next argument since we read it
		}

		if ((strcmp(argv[i], "-r") == 0) && (i + 1 < argc)) {
			read_pipe = argv[i + 1];
			i++;		// Skip next argument since we read it
		}

		if ((strcmp(argv[i], "-o") == 0) && (i + 1 < argc)) {
			operations_file = argv[i + 1];
			i++;		// Skip next argument since we read it
		}

	}

	if (write_pipe == NULL || read_pipe == NULL) {
		fprintf(stderr, "Usage: %s -w <write_pipe> -r <read_pipe> [-o <ops_file>]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Open named pipes
	if ((writefd = open(write_pipe, O_WRONLY)) < 0) {
		perror("Console can not open write pipe!\n");
	}

	if ((readfd = open(read_pipe, O_RDONLY)) < 0) {
		perror("Console can not open read pipe!\n");
	}

	console(writefd, readfd, operations_file);

	close(writefd);
	close(readfd);
	exit(0);
}