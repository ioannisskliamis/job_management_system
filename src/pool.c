#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <sys/wait.h>
#include <time.h>

#include "pool.h"
#include "messages.h"

void pool(char* in_pipe, char* out_pipe, int jobs_pools, char* path) {
	int readfd, writefd;									// Open named pipes for communication with coordinator
	readfd = open(in_pipe, O_RDONLY | O_NONBLOCK);
	writefd = open(out_pipe, O_WRONLY);
    
	int jobs_completed = 0;									// Keep track how many jobs have been assigned to this pool
	int active_pids[jobs_pools];
	int active_job_ids[jobs_pools];
	int active_count = 0;
	int flag = 1;											// For shutdown signal

	struct pollfd fds[1];
	fds[0].fd = readfd;
	fds[0].events = POLLIN;

	while (flag && jobs_completed < jobs_pools) {			// While loop until we reach the max number of jobs the user specified
		// poll() with a 100ms timeout.
		// It wakes up if the Coordinator talks or every 100ms to check if any of the children (running jobs) have finished
		int ret = poll(fds, 1, 100);

		if (ret < 0) {

			if (errno == EINTR)
				continue;
			
			perror("Pool poll error");
			break;
		}

        if (ret > 0 && (fds[0].revents & POLLIN)) {			// If Coordinator sent a message
			jms_message msg;								// Read message from coordinator

			while (read(readfd, &msg, sizeof(jms_message)) > 0) {

				if (msg.type == SUBMIT) {
					// Time
					time_t now = time(NULL);
					struct tm t_copy;
					localtime_r(&now, &t_copy);
					char date[16];
					char time_str[16];
					strftime(date, sizeof(date), "%Y%m%d", &t_copy);
					strftime(time_str, sizeof(time_str), "%H%M%S", &t_copy);

					// Create directory for job
					char dir[512];
					snprintf(dir, sizeof(dir), "%s/outputs_%d_%d_%s_%s", path, msg.job_id, getpid(), date, time_str);

					if ((mkdir(dir, 0777) == -1) && (errno != EEXIST)) {
						perror("Error creating directory for job!\n");
						exit(EXIT_FAILURE);
					}

					pid_t pid = fork();

					if (pid == 0) {							// Child process (job)
						// Create file names for files inside job direcotry
						char out_path[600];
						char err_path[600];
						snprintf(out_path, sizeof(out_path), "%s/stdout_%d", dir, msg.job_id);
						snprintf(err_path, sizeof(err_path), "%s/stderr_%d", dir, msg.job_id);

						// Open file descriptors
						int out_fd = open(out_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
						int err_fd = open(err_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);

						if (out_fd < 0 || err_fd < 0)
							exit(1);

						// Redirect outputs
						dup2(out_fd, STDOUT_FILENO);
						dup2(err_fd, STDERR_FILENO);

						// Close file descriptors
						close(out_fd);
						close(err_fd);

						// Redirect stdin to /dev/null so interactive jobs fail cleanly
						int dev_null = open("/dev/null", O_RDONLY);
						
						if (dev_null >= 0) {
							dup2(dev_null, STDIN_FILENO);
							close(dev_null);
						}

						// Exec() for job
						execl("/bin/sh", "sh", "-c", msg.job, NULL);
						exit(1);
					} else {								// Parent process (pool)
						active_pids[active_count] = pid;
						active_job_ids[active_count] = msg.job_id;
						active_count++;						// Increase count of jobs that are running by this pool
					}

				} else if (msg.type == SUSPEND || msg.type == RESUME) {

					// Find the pid associated with the job_id the coordinator sent
					for (int i = 0; i < active_count; i++) {

						if (active_job_ids[i] == msg.job_id) {

							if (msg.type == SUSPEND) {	// Suspend child process (job)
								kill(active_pids[i], SIGSTOP);
							} else {					// Resume child process (job)
								kill(active_pids[i], SIGCONT);
							}

							break;
						}

					}

				} else if (msg.type == SHUTDOWN) {

					// Send SIGTERM to all children (jobs)
					for (int i = 0; i < active_count; i++) {
						kill(active_pids[i], SIGTERM);
					}

					// Wait for them to die
					for (int i = 0; i < active_count; i++) {
						waitpid(active_pids[i], NULL, 0);
					}

					// Change flag to leave loop and end pool process
					flag = 0;
					break;
				}

			}

		}

		// Check for finished children (jobs)
		int status;
		pid_t finished_pid;

		while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) {

			for (int i = 0; i < active_count; i++) {

				if (active_pids[i] == finished_pid) {
					jms_message done_msg;				// Send FINISHED notification to Coordinator
					memset(&done_msg, 0, sizeof(jms_message));
					done_msg.type = DONE; 				// Status update type
					done_msg.job_id = active_job_ids[i];
					write(writefd, &done_msg, sizeof(jms_message));

					// Remove from tracking list
					active_pids[i] = active_pids[active_count - 1];
					active_job_ids[i] = active_job_ids[active_count - 1];
					active_count--;
					jobs_completed++;
					break;
				}

			}

		}

	}

	close(readfd);
	close(writefd);
	exit(0);
}