#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <time.h>
#include <signal.h>

#include "messages.h"
#include "coordinator.h"
#include "jobs_list.h"
#include "pool.h"

int next_job_id = 1;												// Global counter to determine the job ids

void coordinator(int readfd, int writefd, int jobs_pools, char* path) {
	jms_message incoming_msg;										// Message that the coordinator reads from console
	jms_message msg_to_send;										// Message to be sent to console

	int current_pool_capacity = 10;
	int* pool_write_fds = malloc(current_pool_capacity * sizeof(int));	// Array with file descriptors for pipes that send messages to pools
	int* pool_read_fds = malloc(current_pool_capacity * sizeof(int));	// Array with file descriptors for pipes that send messages from pools to coordinator
	int num_active_pools = 0;

	JobEntry* jobs_list = NULL;										// Create jobs list
	pid_t* pool_pids = malloc(current_pool_capacity * sizeof(pid_t));	// Array with pool pids

	int* poll_jobs_count = malloc(current_pool_capacity * sizeof(int));		// Keep track how many jobs have been assigned to every pool
	int* pool_active_count = malloc(current_pool_capacity * sizeof(int));	// Keep track of how many active jobs are there in every pool

	for (int i = 0; i < current_pool_capacity; i++)
		poll_jobs_count[i] = 0;

	int* pool_alive = malloc(current_pool_capacity * sizeof(int));		// Array with pools that are alive

	for (int i = 0; i < current_pool_capacity; i++)
		pool_alive[i] = 1;

	// Main coordinator loop
	while (1) {
		// Structure used by poll() system call
		struct pollfd* fds = malloc((num_active_pools + 1) * sizeof(struct pollfd));

		if (!fds) {
			perror("malloc error!");
			break;
		}

        fds[0].fd = readfd;											// Watch the Console
        fds[0].events = POLLIN;

        // Watch all active Pools
        for (int i = 0; i < num_active_pools; i++) {
            fds[i+1].fd = pool_read_fds[i];
            fds[i+1].events = POLLIN;
        }

        int activity = poll(fds, num_active_pools + 1, -1);			// Block here until someone talks, -1 means no timeout.

        if (activity < 0) {
        	free(fds);

            if (errno == EINTR)										// Ignore interrupts, like signals
            	continue;

            perror("poll error!");
            break;
        }

        // Check console
        if (fds[0].revents & POLLIN) {
            ssize_t n = read(readfd, &incoming_msg, sizeof(jms_message));

			if (n <= 0) {											// Console disconnected without shutdown — clean up and exit
				free(fds);
				break;
			}

			switch (incoming_msg.type) {
				case SUBMIT: {
					int target_id = -1;

					for (int i = 0; i < num_active_pools; i++) {	// Find pool that's available to run job

						if (poll_jobs_count[i] < jobs_pools) {		// Pool is not yet retired
							target_id = i;
							break;
						}

					}

					// If there is no available pool for given job, then create a new one
					if (target_id == -1) {

						if (num_active_pools >= current_pool_capacity) {
							current_pool_capacity *= 2;				// Double the capacity
							pool_pids = realloc(pool_pids, current_pool_capacity * sizeof(pid_t));
							pool_write_fds = realloc(pool_write_fds, current_pool_capacity * sizeof(int));
							pool_read_fds = realloc(pool_read_fds, current_pool_capacity * sizeof(int));
							poll_jobs_count = realloc(poll_jobs_count, current_pool_capacity * sizeof(int));
							pool_alive = realloc(pool_alive, current_pool_capacity * sizeof(int));
							pool_active_count = realloc(pool_active_count, current_pool_capacity * sizeof(int));

							// Initialize the new portion of poll_jobs_count
							for (int i = num_active_pools; i < current_pool_capacity; i++) {
								poll_jobs_count[i] = 0;
							}
						}

						// Generate names for named pipes for new pool and create them
						char in_pipe[FIFO_BUFFER_SIZE];
						char out_pipe[FIFO_BUFFER_SIZE];
						snprintf(in_pipe, sizeof(in_pipe), "%s/pool_%d_in", path, num_active_pools);
						snprintf(out_pipe, sizeof(out_pipe), "%s/pool_%d_out", path, num_active_pools);

						if ((mkfifo(in_pipe, 0666) < 0) && (errno != EEXIST)) {
							perror("Can not create pipe between pool and coordinator!");
						}

						if ((mkfifo(out_pipe, 0666) < 0) && (errno != EEXIST)) {
							perror("Can not create pipe between coordinator and pool!");
						}

						pid_t pid = fork();							// Create new child process

						if (pid == 0) {								// Child process
							pool(in_pipe, out_pipe, jobs_pools, path);	// Pool function
							exit(0);
						} else {									// Parent process
							pool_pids[num_active_pools] = pid;
							pool_write_fds[num_active_pools] = open(in_pipe, O_WRONLY);
							pool_read_fds[num_active_pools] = open(out_pipe, O_RDONLY);
							target_id = num_active_pools;
							poll_jobs_count[target_id] = 0; 		// Initialize count
							num_active_pools++;
						}
					}

					int job_id = next_job_id++;						// Assign the Job ID and record it in our internal list
					insert_job(&jobs_list, job_id, pool_pids[target_id], ACTIVE);
					incoming_msg.job_id = job_id;					// Prepare the message for the Pool
					incoming_msg.pid = pool_pids[target_id];

					// Write to the Pool's input pipe
					write(pool_write_fds[target_id], &incoming_msg, sizeof(jms_message));
					poll_jobs_count[target_id]++;					// Update the pool's job count
					pool_active_count[target_id]++;					// Update the pools's active count

					// Tell the console that the job has been submitted
					msg_to_send.type = SUBMIT;
					msg_to_send.job_id = job_id;
					msg_to_send.pid = pool_pids[target_id];
					write(writefd, &msg_to_send, sizeof(jms_message));
					break;
				}

				case STATUS: {
					int search_id = incoming_msg.job_id;			// Search for user specified job id
					JobEntry* found = find_job(jobs_list, search_id);

					if (found != NULL) {							// If the job id indeed exists, prepare message to be sent to console
						memset(&msg_to_send, 0, sizeof(jms_message));
						msg_to_send.type = STATUS;
						msg_to_send.job_id = found->job_id;
						msg_to_send.status = found->job_status;
						time_t now = time(NULL);
						int seconds_running = found->elapsed_before_suspend + (int) (now - found->start);
						msg_to_send.seconds = seconds_running;
					} else {										// If the job id does not exist
						msg_to_send.type = NOT_FOUND;
					}

					// Send message to console
					write(writefd, &msg_to_send, sizeof(jms_message));
					break;
				}

				case STATUS_ALL: {
					JobEntry* current = jobs_list;
					time_t current_time = time(NULL);

					while (current != NULL) {						// Send each job's info in a message

						if (incoming_msg.seconds == -1 || (current_time - current->start) <= incoming_msg.seconds) {
							msg_to_send.type = STATUS_ALL;
							msg_to_send.job_id = current->job_id;
							msg_to_send.status = current->job_status;
							time_t now = time(NULL);
							msg_to_send.seconds = current->elapsed_before_suspend + (int)(now - current->start);
							write(writefd, &msg_to_send, sizeof(jms_message));
						}

						current = current->next;
					}

					msg_to_send.type = FINISH;						// Send finish message, telling the console to stop reading from pipe
					msg_to_send.job_id = -1;
					write(writefd, &msg_to_send, sizeof(jms_message));
					break;
				}

				case SHOW_ACTIVE: {
					JobEntry* current = jobs_list;

					while (current != NULL) {						// Loop through the list of jobs

						if (current->job_status == ACTIVE) {		// Find active jobs and send them, each one using a different message
							msg_to_send.type = SHOW_ACTIVE;
							msg_to_send.job_id = current->job_id;
							msg_to_send.status = current->job_status;
							write(writefd, &msg_to_send, sizeof(jms_message));
						}

						current = current->next;
					}

					msg_to_send.type = FINISH;						// Send finish message, telling the console to stop reading from pipe
					msg_to_send.job_id = -1;
					write(writefd, &msg_to_send, sizeof(jms_message));
					break;
				}

				case SHOW_POOLS: {

					for (int i = 0; i < num_active_pools; i++) {	// Send a message for each pool and the number of jobs it has done
						msg_to_send.type = SHOW_POOLS;
						msg_to_send.pid = pool_pids[i];
						msg_to_send.jobs_count = pool_active_count[i];	// Send active jobs
						write(writefd, &msg_to_send, sizeof(jms_message));
					}

					msg_to_send.type = FINISH;						// Send FINISH message to console signaling the end of messages
					msg_to_send.job_id = -1;
					write(writefd, &msg_to_send, sizeof(jms_message));
					break;
				}

				case SHOW_FINISHED: {
					JobEntry* current = jobs_list;

					while (current != NULL) {						// Loop through the list of jobs

						if (current->job_status == FINISHED) {		// Find finished jobs and send them, each one using a different message
							msg_to_send.type = SHOW_FINISHED;
							msg_to_send.job_id = current->job_id;
							msg_to_send.status = current->job_status;
							write(writefd, &msg_to_send, sizeof(jms_message));
						}

						current = current->next;
					}

					msg_to_send.type = FINISH;						// Send finish message, telling the console to stop reading from pipe
					msg_to_send.job_id = -1;
					write(writefd, &msg_to_send, sizeof(jms_message));
					break;
				}

				case SUSPEND: {
					JobEntry* found = find_job(jobs_list, incoming_msg.job_id);

					if (found != NULL) {
						found->job_status = SUSPENDED;
						// Save time elapsed before suspending
						time_t now = time(NULL);
						found->elapsed_before_suspend += (int) (now - found->start);
						int pool_idx = -1;							// Find which pool index owns this pool_id

						for (int i = 0; i < num_active_pools; i++) {

							if (pool_pids[i] == found->pool_id) {
								pool_idx = i;
								break;
							}

						}

						if (pool_idx != -1) {						// Forward the exact same message you got from console to the pool
							write(pool_write_fds[pool_idx], &incoming_msg, sizeof(jms_message));
        				}
        
						// Prepare success response for console
						msg_to_send.type = SUSPEND;
						msg_to_send.job_id = found->job_id;
					} else {
						msg_to_send.type = NOT_FOUND;
					}

					write(writefd, &msg_to_send, sizeof(jms_message));
					break;
				}

				case RESUME: {
					JobEntry* found = find_job(jobs_list, incoming_msg.job_id);

					if (found != NULL) {
						found->job_status = ACTIVE;
						found->start = time(NULL);
						int pool_idx = -1;							// Find which pool index owns this pool_id

						for (int i = 0; i < num_active_pools; i++) {

							if (pool_pids[i] == found->pool_id) {
								pool_idx = i;
								break;
							}

						}

						if (pool_idx != -1) {						// Forward the exact same message you got from console to the Pool
							write(pool_write_fds[pool_idx], &incoming_msg, sizeof(jms_message));
        				}
        
						// Prepare success response for console
						msg_to_send.type = RESUME;
						msg_to_send.job_id = found->job_id;
					} else {
						msg_to_send.type = NOT_FOUND;
					}

					write(writefd, &msg_to_send, sizeof(jms_message));
					break;
				}

				case SHUTDOWN: {
					int total_finished = 0;
					int total_active = 0;
					JobEntry* curr = jobs_list;						// Calculate stats from jobs_list

					while (curr != NULL) {

						if (curr->job_status == FINISHED) {
							total_finished++;
						} else {
							total_active++;
						}

						curr = curr->next;
					}

					jms_message sd_msg;
					sd_msg.type = SHUTDOWN;

					for (int i = 0; i < num_active_pools; i++) {

						if (!pool_alive[i]) {
							close(pool_write_fds[i]);
							close(pool_read_fds[i]);
							char in_pipe[FIFO_BUFFER_SIZE];
							char out_pipe[FIFO_BUFFER_SIZE];
							snprintf(in_pipe, sizeof(in_pipe), "%s/pool_%d_in", path, i);
							snprintf(out_pipe, sizeof(out_pipe), "%s/pool_%d_out", path, i);
							unlink(in_pipe);
							unlink(out_pipe);
							continue;
						}

						write(pool_write_fds[i], &sd_msg, sizeof(jms_message));
						int waited = 0;

						for (int tries = 0; tries < 50; tries++) {

							if (waitpid(pool_pids[i], NULL, WNOHANG) > 0) {
								waited = 1;
								break;
							}

							usleep(100000);
						}

						if (!waited) {
							kill(pool_pids[i], SIGKILL);
							waitpid(pool_pids[i], NULL, 0);
						}

						close(pool_write_fds[i]);
						close(pool_read_fds[i]);
						char in_pipe[FIFO_BUFFER_SIZE];
						char out_pipe[FIFO_BUFFER_SIZE];
						snprintf(in_pipe, sizeof(in_pipe), "%s/pool_%d_in", path, i);
						snprintf(out_pipe, sizeof(out_pipe), "%s/pool_%d_out", path, i);
						unlink(in_pipe);
						unlink(out_pipe);
					}
					free(pool_alive);

					// Send final shutdown message to console
					msg_to_send.type = SHUTDOWN;
					msg_to_send.jobs_count = total_finished;
					msg_to_send.active_count = total_active;
					write(writefd, &msg_to_send, sizeof(jms_message));

					// Free
					free(pool_write_fds);
					free(pool_read_fds); 
					free(pool_pids);
					free(poll_jobs_count);
					free(pool_active_count);
					exit(0);										// Exit

				}

				default:											// Added this due to compiler warnings
					break;

			
			}

		}

		// Read from pools
		for (int i = 0; i < num_active_pools; i++) {

			if (fds[i+1].revents & POLLHUP) {  						// Pool pipe closed
				pool_alive[i] = 0;
			}

			if (fds[i+1].revents & POLLIN) {
				jms_message pool_msg;

				if (read(pool_read_fds[i], &pool_msg, sizeof(jms_message)) > 0) {
					
					if (pool_msg.type == DONE) {
						JobEntry* job = find_job(jobs_list, pool_msg.job_id);

						if (job != NULL) {							// Find job and update its status
							job->job_status = FINISHED;
							job->end = time(NULL);
						}

						// Find which pool this belongs to and decrement active jobs
						for (int j = 0; j < num_active_pools; j++) {

							if (pool_pids[j] == job->pool_id) {
								pool_active_count[j]--;
								break;
							}

						}

					}

				} else {
					pool_alive[i] = 0;  							// Fallback
				}

			}

		}

       	free(fds);
	}

}