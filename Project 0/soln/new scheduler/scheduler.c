/* Fill in your Name and GNumber in the following two comment fields
 * Name:Siam Al-mer Chowdhury
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "structs.h"
#include "constants.h"
#include "scheduler.h"

/* Initialize the Schedule Struct
 * Follow the specification for this function.
 * Returns a pointer to the new Schedule or NULL on any error.
 */
Schedule *scheduler_init() {
	// create Schedule struct
	Schedule * scheduler = (Schedule *)malloc(sizeof(Schedule));
	if (!scheduler)
		return NULL;
	// allocate Schedule Queues
	scheduler->ready_queue = (Queue *)malloc(sizeof(Queue));
	scheduler->defunct_queue = (Queue *)malloc(sizeof(Queue));
	scheduler->stopped_queue = (Queue *)malloc(sizeof(Queue));
	// if allocation is failed, return NULL
	if (!scheduler && !scheduler->ready_queue && !scheduler->defunct_queue && !scheduler->stopped_queue) {
		scheduler_free(scheduler);
		return NULL;
	}
	// initialize queues
	scheduler->ready_queue->head = NULL;
	scheduler->ready_queue->count = 0;
	scheduler->defunct_queue->head = NULL;
	scheduler->defunct_queue->count = 0;
	scheduler->stopped_queue->head = NULL;
	scheduler->stopped_queue->count = 0;
	return scheduler;
}

/* Add the process into the appropriate linked list.
 * Follow the specification for this function.
 * Returns a 0 on success or a -1 on any error.Z
 */
int scheduler_add(Schedule *schedule, Process *process) {
	if (!schedule || !process)
		return -1;
	// Do the operations based on which of these...
	int state = (process->flags & 0x7c000000) >> 26;
	switch (state)
	{
	case CREATED:
		// Change the state to READY
		state = READY;
		// Then, insert it into the front of the ReadyQueue
		if (!schedule->ready_queue)
			return -1;
		process->next = schedule->ready_queue->head;
		schedule->ready_queue->head = process;
		schedule->ready_queue->count++;
		break;
	case READY:
		// check its time_remaining
		if (process->time_remaining > 0) {
			if (!schedule->ready_queue)
				return -1;
			process->next = schedule->ready_queue->head;
			schedule->ready_queue->head = process;
			schedule->ready_queue->count++;
		}
		else {
			state = DEFUNCT;
			if (!schedule->defunct_queue)
				return -1;
			process->next = schedule->defunct_queue->head;
			schedule->defunct_queue->head = process;
			schedule->defunct_queue->count++;
		}
		break;
	case DEFUNCT:
		// insert it into the front of the Defunct Queue
		if (!schedule->defunct_queue)
			return -1;
		process->next = schedule->defunct_queue->head;
		schedule->defunct_queue->head = process;
		schedule->defunct_queue->count++;
		break;
	default:
		// if the process arrives in any other state, return as an error
		return -1;
	}
	process->flags &= 0x83ffffff;
	process->flags |= (state << 26);
	return 0;
}

/* Move the process with matching pid from Ready to Stopped.
 * Change its State to Stopped.
 * Follow the specification for this function.
 * Returns a 0 on success or a -1 on any error.
 */
int scheduler_stop(Schedule *schedule, int pid) {
	int exit_code = -1;
	// if the Ready Queue and the Stopped Queue are NULL, return error
	if (!schedule->ready_queue || !schedule->stopped_queue)
		return -1;
	// Find the process with matching pid from the Ready Queue
	Process * process = schedule->ready_queue->head;
	Process * prev_process = NULL;
	int found = 0;
	while (process) {
		if (process->pid == pid) {
			found = 1;
			break;
		}
		prev_process = process;
		process = prev_process->next;
	}
	if (found == 1) {
		// Once found, remove the process from the Ready Queue
		if (process == schedule->ready_queue->head)
			schedule->ready_queue->head = process->next;
		else
			prev_process->next = process->next;
		schedule->ready_queue->count--;
		// Set the process state to STOPED
		process->flags &= 0x83ffffff;
		process->flags |= (STOPPED << 26);
		// Insert that process into the front of the Stopped Queue
		process->next = schedule->stopped_queue->head;
		schedule->stopped_queue->head = process;
		schedule->stopped_queue->count++;
		exit_code = 0;
	}
	return exit_code;
}

/* Move the process with matching pid from Stopped to Ready.
 * Change its State to Ready.
 * Follow the specification for this function.
 * Returns a 0 on success or a -1 on any error.
 */
int scheduler_continue(Schedule *schedule, int pid) {
	int exit_code = -1;
	// if the Ready Queue and the Stopped Queue are NULL, return error
	if (!schedule->ready_queue || !schedule->stopped_queue)
		return -1;
	// Find the process with matching pid from the Stopped Queue
	Process * process = schedule->stopped_queue->head;
	Process * prev_process = NULL;
	int found = 0;
	while (process) {
		if (process->pid == pid) {
			found = 1;
			break;
		}
		prev_process = process;
		process = prev_process->next;
	}
	if (found == 1) {
		// Once found, remove the process from the Stopped Queue
		if (process == schedule->stopped_queue->head)
			schedule->stopped_queue->head = process->next;
		else
			prev_process->next = process->next;
		schedule->stopped_queue->count--;
		// Set the process state to READY
		process->flags &= 0x83ffffff;
		process->flags |= (READY << 26);
		// Insert that process into the front of the Ready Queue
		process->next = schedule->ready_queue->head;
		schedule->ready_queue->head = process;
		schedule->ready_queue->count++;
		exit_code = 0;
	}
	return exit_code;
}

/* Remove the process with matching pid from Defunct.
 * Follow the specification for this function.
 * Returns its exit code (from flags) on success or a -1 on any error.
 */
int scheduler_reap(Schedule *schedule, int pid) {
	int exit_code = -1;
	// if Defunct Queue is NULL, return error
	if (!schedule->defunct_queue)
		return -1;
	// find the process with matching pid from the Defunct Queue
	Process * process = schedule->defunct_queue->head;
	Process * prev_process = NULL;
	int found = 0;
	while (process) {
		if (process->pid == pid) {
			found = 1;
			break;
		}
		prev_process = process;
		process = prev_process->next;
	}
	if (found == 1) {
		// Once found, remove the process from the Defunct Queue
		if (process == schedule->defunct_queue->head)
			schedule->defunct_queue->head = process->next;
		else
			prev_process->next = process->next;
		schedule->defunct_queue->count--;
		// Set the process state to TERMINATED
		process->flags &= 0x83ffffff;
		process->flags |= (TERMINATED << 26);
		// Extract its exit_code from the flags
		exit_code = process->flags & (0x03ffffff);
		// Free the process
		if (process->command)
			free(process->command);
		free(process);
	}
	return exit_code;
}

/* Create a new Process with the given information.
 * - Malloc and copy the command string, don't just assign it!
 * Set the STATE_CREATED flag.
 * If is_sudo, also set the PF_SUPERPRIV flag.
 * Follow the specification for this function.
 * Returns the Process on success or a NULL on any error.
 */
Process *scheduler_generate(char *command, int pid, int base_priority, 
                            int time_remaining, int is_sudo) {
	int bitflags = 0;
	// create Process struct
	Process *process = (Process *)malloc(sizeof(Process));
	if (!process)
		return NULL;
	// malloc and copy the command string
	process->command = (char *) malloc(strlen(command) + 1);
	if (!process->command) {
		free(process);
		return NULL;
	}
	strcpy(process->command, command);
	// Set the fields of pid and so on
	process->pid = pid;
	process->base_priority = base_priority;
	process->time_remaining = time_remaining;
	// For the cur_priority field, set it to ...
	process->cur_priority = base_priority;
	// If is_sudo is 1, set the SUDO bit 
	if (is_sudo == 1)
		bitflags |= SUDO;
	// Set the CREATED bit to 1 and all other state bits to 0
	bitflags |= CREATED;
	bitflags <<= 26;
	// Make sure the exit_code portion of the flags starts at 0
	bitflags &= 0xfc000000;
	process->flags = bitflags;
	process->next = NULL;
	return process;
}

/* Select the next process to run from Ready Queue.
 * Follow the specification for this function.
 * Returns the process selected or NULL if none available or on any errors.
 */
Process *scheduler_select(Schedule *schedule) {
	Process *process = NULL, *front_process = NULL;
	Process *curr_process = NULL, *prev_process = NULL;
	int lowest_priority = 139;
	// if Reeay Queue is NULL, return error
	if (!schedule->ready_queue)
		return NULL;
	// Remove the process with the lowest cur_priority in the Ready Queue
	curr_process = schedule->ready_queue->head;
	while (curr_process) {
		if (curr_process->cur_priority < lowest_priority) {
			process = curr_process;
			front_process = prev_process;
			lowest_priority = curr_process->cur_priority;
		}
		prev_process = curr_process;
		curr_process = prev_process->next;
	}
	if (!process)
		return NULL;
	if (process == schedule->ready_queue->head)
		schedule->ready_queue->head = process->next;
	else
		front_process->next = process->next;
	schedule->ready_queue->count--;
	// For that process you removed, set its cur_priority equal to its base_priority
	process->next = NULL;
	process->cur_priority = process->base_priority;
	// Then subtract 1 from cur_priority of all remaining structs in the Ready Queue
	curr_process = schedule->ready_queue->head;
	while (curr_process) {
		if (curr_process->cur_priority > 0)
			curr_process->cur_priority--;
		curr_process = curr_process->next;
	}
	return process;
}

/* Returns the number of items in a given Linked List (Queue) (Queue)
 * Follow the specification for this function.
 * Returns the count of the Linked List, or -1 on any errors.
 */
int scheduler_count(Queue *ll) {
	if (!ll)
		return -1;
	else
		return ll->count;
}

/* Completely frees all allocated memory in the scheduler
 * Follow the specification for this function.
 */
void scheduler_free(Schedule *scheduler) {
	Process *process = NULL, *next_process = NULL;
	if (!scheduler)
		return;
	// Free the Ready Queue
	if (scheduler->ready_queue) {
		process = scheduler->ready_queue->head;
		while (process) {
			next_process = process->next;
			if (process->command)
				free(process->command);
			free(process);
			process = next_process;
		}
		free(scheduler->ready_queue);
	}
	// Free the Defunct Queue
	if (scheduler->defunct_queue) {
		process = scheduler->defunct_queue->head;
		while (process) {
			next_process = process->next;
			if (process->command)
				free(process->command);
			free(process);
			process = next_process;
		}
		free(scheduler->defunct_queue);
	}
	// Free the Stopped Queue
	if (scheduler->stopped_queue) {
		process = scheduler->stopped_queue->head;
		while (process) {
			next_process = process->next;
			if (process->command)
				free(process->command);
			free(process);
			process = next_process;
		}
		free(scheduler->stopped_queue);
	}
	// Free the Schedule
	free(scheduler);
}
