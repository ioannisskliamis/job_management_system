#ifndef JOBS_LIST_H
#define JOBS_LIST_H

#include <sys/types.h>
#include <time.h>

#include "messages.h"

// Job entry struct for list that holds all the jobs
typedef struct JobEntry {
	int job_id;
	time_t start;					// Job started running
	time_t end;						// Job finished running (0 if still running)
	int elapsed_before_suspend;		// Save seconds the job ran before suspending it
	job_status job_status;
	pid_t pool_id;
	struct JobEntry* next;
} JobEntry;

JobEntry* insert_job(JobEntry** head, int id, pid_t pool_id, job_status status);
void delete_job(JobEntry** head, int id);
void destroy_list(JobEntry** head);
JobEntry* find_job(JobEntry* head, int id);

#endif