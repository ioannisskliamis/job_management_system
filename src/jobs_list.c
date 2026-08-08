#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

#include "jobs_list.h"

JobEntry* insert_job(JobEntry** head, int id, pid_t pool_id, job_status status) {
	JobEntry* new_node = malloc(sizeof(JobEntry));

	// Initialize the data
	new_node->job_id = id;
	new_node->pool_id = pool_id;
	new_node->start = time(NULL);
	new_node->elapsed_before_suspend = 0;
	new_node->end = 0;
	new_node->job_status = status;
	new_node->next = NULL;

	if (*head == NULL) {						// If list is empty, make this the head
		*head = new_node;
	} else {									// Otherwise, traverse to the end and attach it
		JobEntry* current = *head;

		while (current->next != NULL) {
			current = current->next;
		}

		current->next = new_node;
	}

	return new_node;
}

void delete_job(JobEntry** head, int id) {
	JobEntry* temp = *head;
	JobEntry* prev = NULL;

	if (temp != NULL && temp->job_id == id) {	// Check if the head node itself holds the ID to be deleted
		*head = temp->next; 					// Changed head
		free(temp);         					// Free memory
		return;
	}

    // Search for the ID to be deleted, keep track of the previous node
	while (temp != NULL && temp->job_id != id) {
		prev = temp;
		temp = temp->next;
	}

    // If ID was not present in list
	if (temp == NULL)
    	return;

	prev->next = temp->next;					// Unlink the node from linked list
	free(temp);									// Free memory
}

void destroy_list(JobEntry** head) {
	JobEntry* current = *head;
	JobEntry* next_node;

	while (current != NULL) {
		next_node = current->next;				// Save the pointer to the next node
		free(current);							//Free the memory of the current node
		current = next_node;					// Move to the next node
	}

	*head = NULL;								// Finally, set the head pointer to NULL to indicate the list is empty
}

JobEntry* find_job(JobEntry* head, int id) {
	JobEntry* current = head;

	while (current != NULL) {					// Loop through the list to find the entry corresponding to given job id

		if (current->job_id == id) {
			return current;
		}

		current = current->next;
	}

	return NULL;								// Return NULL if entry with given user id is not present in list
}