#ifndef MESSAGES_H
#define MESSAGES_H

#include <sys/types.h>

#define BUFFER_SIZE 	 256
#define FIFO_BUFFER_SIZE 256

// Enum for message types
// Finish message type is a special type that is sent only when coordinator needs to send multiple messages at once
// Not Found message type is a special type that is sent only when the coordinatro doesnt manage do find information corresponding to something the user asked
// i.e. a job id that doesnt exist
// Done message type is a special type that is sent by the pools when a job is finished to the coordinator in order for the coordinator to update
// the status of the job to finished
typedef enum {
	SUBMIT,
	STATUS,
	STATUS_ALL,
	SHOW_ACTIVE,
	SHOW_POOLS,
	SHOW_FINISHED,
	SUSPEND,
	RESUME,
	SHUTDOWN,
	FINISH,
	NOT_FOUND,
	DONE
} message_type;

// Enum for job status
typedef enum {
	ACTIVE,
	FINISHED,
	SUSPENDED
} job_status;

// Message struct for communication between coordinator and console
typedef struct {
	message_type type;
	int job_id;
	char job[BUFFER_SIZE];
	pid_t pid;
	job_status status;
	int jobs_count;
	int active_count;
	int seconds;
} jms_message;

#endif