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

message_type get_message_type(char* command) {					// Find message type for message to send, according to user input

	if (strcmp(command, "submit") == 0)
		return SUBMIT;

	if (strcmp(command, "status") == 0)
		return STATUS;

	if (strcmp(command, "status-all") == 0)
		return STATUS_ALL;

	if (strcmp(command, "show-active") == 0)
		return SHOW_ACTIVE;

	if (strcmp(command, "show-pools") == 0)
		return SHOW_POOLS;

	if (strcmp(command, "show-finished") == 0)
		return SHOW_FINISHED;

	if (strcmp(command, "suspend") == 0)
		return SUSPEND;

	if (strcmp(command, "resume") == 0)
		return RESUME;

	if (strcmp(command, "shutdown") == 0)
		return SHUTDOWN;

	return -1;															// In case of error due to bad user input

}

void communication_with_coord(int writefd, int readfd, char* input) {
	jms_message incoming_msg;											// Message that the console reads from coordinator
	jms_message msg_to_send;											// Message to be sent to coordinator
	memset(&msg_to_send, 0, sizeof(jms_message));
	ssize_t bytes_sent;

	input[strcspn(input, "\n")] = 0;

	if (strlen(input) == 0) 											// Empty line (accidentally)
		return;

	char command[32];
	char argument[BUFFER_SIZE] = ""; 									// Initialize to empty string
	int items = sscanf(input, "%s %[^\n]", command, argument);
	message_type type = get_message_type(command);						// Find the type of command the user has sent

	if ((int)type == -1) {												// Incorrect user command case
		printf("Unknown command: %s\n", command);
		return;
	}

	msg_to_send.type = type;

	// Create the message to send to coordinator
	switch (type) {
		case SUBMIT:													// Send submit message type
			if (items > 1) {
				strncpy(msg_to_send.job, argument, BUFFER_SIZE - 1);	// Send job arguments
			} else {
				printf("Error: submit command needs job arguments!\n");
				return;
			}

			break;

		case STATUS:													// Send status message type
			if (items > 1) {											// The argument is the job id
				char *endptr;											// msg_to_send.job_id = atoi(argument);
				errno = 0;
				long val = strtol(argument, &endptr, 10);

				if (errno != 0 || endptr == argument || val <= 0) {
					printf("Error: invalid job ID '%s'\n", argument);
					return;
				}

				msg_to_send.job_id = (int)val;
            } else {
                printf("Error: %s requires a <job_id>!\n", command);
                return;
            }

            break;

		case STATUS_ALL:												// Send status-all message type
			if (items > 1) {											// The argument is the n
                char *endptr;											// msg_to_send.seconds = atoi(argument);
				errno = 0;
				long val = strtol(argument, &endptr, 10);

				if (errno != 0 || endptr == argument || val < 0) {
					printf("Error: invalid n '%s'\n", argument);
					return;
				}

				msg_to_send.seconds = (int) val;
            } else {
                msg_to_send.seconds = -1;
            }

			break;

		case SHOW_ACTIVE:												// Send show-active message type
			break;

		case SHOW_POOLS:												// Send show-pools message type
			break;

		case SHOW_FINISHED:												// Send show-finished message type
			break;

		case SUSPEND:													// Send suspend message type
			if (items > 1) {											// The argument is the job id
                char *endptr;											// msg_to_send.job_id = atoi(argument);
				errno = 0;
				long val = strtol(argument, &endptr, 10);

				if (errno != 0 || endptr == argument || val <= 0) {
					printf("Error: invalid job ID '%s'\n", argument);
					return;
				}

				msg_to_send.job_id = (int)val;
            } else {
                printf("Error: %s requires a <job_id>!\n", command);
                return;
            }

            break;

		case RESUME:													// Send resume message type
			if (items > 1) {											// The argument is the job id
                char *endptr;											// msg_to_send.job_id = atoi(argument);
				errno = 0;
				long val = strtol(argument, &endptr, 10);

				if (errno != 0 || endptr == argument || val <= 0) {
					printf("Error: invalid job ID '%s'\n", argument);
					return;
				}

				msg_to_send.job_id = (int) val;
            } else {
                printf("Error: %s requires a <job_id>!\n", command);
                return;
            }

            break;

		case SHUTDOWN:													// Send shutdown message type
			break;

		default:														// Added this due to compiler warnings
			break;

	}

	bytes_sent = write(writefd, &msg_to_send, sizeof(jms_message));		// Send the message to the coordinator

	// For command where the coordinator sends multiple messages, print header before reading the actual messages
	// in order to print the header only once and not every time a message is read from pipe from the coordinator to the console
	if (type == SHOW_ACTIVE) {
		printf("Active jobs:\n");
	}

    if (type == SHOW_POOLS) {
    	printf("Pool & NumOfJobs:\n");
    }

    if (type == SHOW_FINISHED) {
    	printf("Finished jobs:\n");
    }

	// Read from coordinator
	while (read(readfd, &incoming_msg, sizeof(jms_message)) > 0) {

		if (incoming_msg.type == FINISH)								// If coordinator finished sending information about all jobs
        	return;														// for command where coordinator needs to send multiple messages

		switch (incoming_msg.type) {									// Read message from coordinator
			case SUBMIT:												// if message type is submit
				printf("JobID: %d, PID: %d\n", incoming_msg.job_id, incoming_msg.pid);
				return;

			case STATUS:												// if message type is status
				printf("JobID %d Status: ", incoming_msg.job_id);

				switch (incoming_msg.status) {							// Print status of job
					case ACTIVE:
						printf("Active (running for %d seconds)\n", incoming_msg.seconds);
						break;	

					case FINISHED:
						printf("Finished\n");
						break;

					case SUSPENDED:
						printf("Suspended\n");
						break;
				}

				return;

			case STATUS_ALL:										// if message type is status-all
				printf("JobID %d Status: ", incoming_msg.job_id);
				
				switch (incoming_msg.status) {						// Print status of job
					case ACTIVE:
						printf("Active (running for %d seconds)\n", incoming_msg.seconds);
						break;	

					case FINISHED:
						printf("Finished\n");
						break;

					case SUSPENDED:
						printf("Suspended\n");
						break;
				}

				break;												// Break instead of return because we may need to keep reading incoming messages

			case SHOW_ACTIVE:										// if message type is show-active
				printf("JobID: %d\n", incoming_msg.job_id);
				break;												// Break instead of return because we may need to keep reading incoming messages

			case SHOW_POOLS:										// if message type is show-pools
				printf("%d %d\n", incoming_msg.pid, incoming_msg.jobs_count);
				break;												// Break instead of return because we may need to keep reading incoming messages

			case SHOW_FINISHED:										// if message type is show-finished
				printf("JobID: %d\n", incoming_msg.job_id);
				break;												// Break instead of return because we may need to keep reading incoming messages

			case SUSPEND:											// if message type is suspend
				printf("Sent suspend signal to JobID %d\n", incoming_msg.job_id);
				return;

			case RESUME:											// if message type is resume
				printf("Sent resume signal to JobID %d\n", incoming_msg.job_id);
				return;

			case SHUTDOWN:											// if message type is shutdown
				printf("Served %d jobs, %d were still in progress\n", incoming_msg.jobs_count, incoming_msg.active_count);
				exit(0);

			case NOT_FOUND:
				printf("Could not retrieve information for what has been asked!\n");
				return;
			
			default:												// Added this due to compiler warnings
				break;

		}
	}

}

void console(int writefd, int readfd, char* operations_file) {
	char line[BUFFER_SIZE];

	// Check if operations file exists and do the jobs it describes
	if (operations_file != NULL) {
		FILE *fp = fopen(operations_file, "r");

		if (fp) {

			while (fgets(line, sizeof(line), fp)) {					// Read from file
				communication_with_coord(writefd, readfd, line);
			}

			fclose(fp);
		} else {
			perror("Could not open operations file!\n");
			exit(EXIT_FAILURE);
		}

    }

	// Main console loop (user input)
	while(1) {
		printf("> ");

		if (fgets(line, sizeof(line), stdin) == NULL)				// Read from user input
			break;

		communication_with_coord(writefd, readfd, line);
	}
}