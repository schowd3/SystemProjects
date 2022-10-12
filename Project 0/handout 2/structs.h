/* Do Not Modify this File */

/* Definitions for Structs for this Project */
#ifndef STRUCTS_H
#define STRUCTS_H


/* Process Struct Definition */
typedef struct process_struct {
  char *command;      /* Process Command */
  int pid;            /* Process ID (unique) */
  int flags;          /* Process Flags */
  int base_priority;  /* Process Base Priority */
  int cur_priority;   /* Process' Modified Priority */
  int time_remaining; /* Time Units Left to Execute */
  struct process_struct *next; 
} Process;

/* Linked List (Queue) Struct Definition */
typedef struct queue_struct {
  Process *head; /* Singly Linked List */
  int count;     /* Number of items in list */
} Queue;

/* Schedule Struct Definition */
typedef struct schedule_struct {
  Queue *ready_queue;   /* Ready Processes */
  Queue *stopped_queue; /* Stopped Processes */
  Queue *defunct_queue; /* Defunct Processes */
} Schedule;

#endif /* STRUCTS_H */
