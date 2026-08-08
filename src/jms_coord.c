#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "messages.h"
#include "coordinator.h"

#define FIFO_TO_COORD	"jms_in"
#define FIFO_TO_CONSOLE "jms_out"
#define PERMS			 0666

int fifo_exists(const char* name) {						// Function to check if named pipes already exist
	struct stat st;
	return (stat(name, &st) == 0 && S_ISFIFO(st.st_mode));
}

int main(int argc, char* argv[]) {
	int readfd, writefd, jobs_pool;
	char* path = NULL;

	// Parse cmd line arguments
	for (int i = 1; i < argc; i++) {

		if ((strcmp(argv[i], "-l") == 0) && (i + 1 < argc)) {
			path = argv[i + 1];
			i++;		// Skip next argument since we read it
		}

		if ((strcmp(argv[i], "-n") == 0) && (i + 1 < argc)) {
			jobs_pool = atoi(argv[i + 1]);
			i++;		// Skip next argument since we read it
		}

	}
	
	if (path == NULL || jobs_pool <= 0) {
		fprintf(stderr, "Usage: %s -l <path> -n <jobs_per_pool>!\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if ((mkdir(path, 0777) == -1) && (errno != EEXIST)) {					// Create temp folder if it doesnt exist
		perror("Error creating temp folder!\n");
		exit(EXIT_FAILURE);
	}

	// Create named pipes in path the user specified
	char fifo_to_coord[FIFO_BUFFER_SIZE];
	char fifo_to_console[FIFO_BUFFER_SIZE];
	snprintf(fifo_to_coord, sizeof(fifo_to_coord), "%s/%s", path, FIFO_TO_COORD);
	snprintf(fifo_to_console, sizeof(fifo_to_console), "%s/%s", path, FIFO_TO_CONSOLE);

	// Check if named pipes alredy exist and delete them
	if (fifo_exists(fifo_to_coord)) {

		if (unlink(fifo_to_coord) != 0) {
			perror("Failed to remove fifo from console to coordinator!\n");
			exit(EXIT_FAILURE);
		}

	}

	if (fifo_exists(fifo_to_console)) {

		if (unlink(fifo_to_console) != 0) {
			perror("Failed to remove fifo from coordinator to console!\n");
			exit(EXIT_FAILURE);
		}

	}

	// Create named pipes
	if ((mkfifo(fifo_to_coord, PERMS) < 0) && (errno != EEXIST)) {
		perror("Can not create fifo from console to coordinator!\n");
	}

	if ((mkfifo(fifo_to_console, PERMS) < 0) && (errno != EEXIST)) {
		perror("Can not create fifo from coordinator to console!\n");
	}

	// Open named pipes
	if ((readfd = open(fifo_to_coord, O_RDONLY | O_NONBLOCK)) < 0) {
		perror("Coordinator can not open read pipe!\n");
	}

	if ((writefd = open(fifo_to_console, O_WRONLY)) < 0) {
		perror("Coordinator can not open write pipe!\n");
	}

	coordinator(readfd, writefd, jobs_pool, path);

	close(readfd);
	close(writefd);
	unlink(fifo_to_coord);
	unlink(fifo_to_console);
	exit(0);
}