/* Do Not Modify this File */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "scheduler.h"
#include "clock.h"
#include "structs.h"
#include "constants.h"

/* Local Globals */
typedef struct pid_tracker_struct {
  int pid;
  int exit_code;
  struct pid_tracker_struct *next;
} Pid_Tracker;

static FILE *fp_trace = NULL;
static Pid_Tracker *pid_tracker = NULL;

/* Local Functions */
static void print_queue(Process *queue);
static int parse_kill(Schedule *schedule, char *trace_buffer);
static int parse_reap(Schedule *schedule, char *trace_buffer);
static int valid_schedule(Schedule *schedule);
static int add(int pid, int code);
static int get_code(int pid);
static Process *parse_command(char *trace_buffer, Schedule *schedule);
//static void dump_schedule(Schedule *schedule, Process *p);
//static void dump_queue(FILE *fp, char *command, Queue *queue);
static void sys_teardown();
static void free_trackers();
static void load_flags(char *ary, int flags);

/* Initialization Steps for the OS Process Simulator */
void sys_init(FILE *fp) {
  int ret = atexit(sys_teardown);
  if(ret != 0) {
    printf("Error registering sys_teardown.  Exiting.\n");
    exit(EXIT_FAILURE);
  }
  fp_trace = fp;
}

/* At Exit: Close the Trace File */
static void sys_teardown() {
  if(fp_trace != NULL) {
    fclose(fp_trace);
  }
}

/* Main Run Loop for the OS Process Simulator
 * Stages of the Simulator:
 *  0) *runs once*
 *  --- Calls Scheduler's Init Function to create the Schedule struct
 *  1) Unload the Running Process from CPU
 *  --- Call Scheduler's Add Function on Process
 *  2) Perform 1 action from the trace file.
 *  --- If Create Process:
 *  ------- A) Call Scheduler's Generate Function with Process Data
 *  ------- B) Call Scheduler's Add Function on Process
 *  --- If kill -STOP: 
 *  ------- Call Scheduler's Stop Function on PID
 *  --- If kill -CONT: 
 *  ------- Call Scheduler's Continue Function on PID
 *  --- If reap: 
 *  ------- Call Scheduler's Reap Function on PID
 *  --- If exit:
 *  ------- Exits after Advance Time stage.
 *  3) Get New Process to Run
 *  --- Call Scheduler's Select function to get Process
 *  --- Adds Process to the CPU Running Set (if one selected)
 *  4) Advance Time
 *  --- Advance the Clock by 1
 *  --- Decrement time remaining on Running Process
 *  --- If time remaining is 0
 *  ------- Add exit code to flags
 *  --- If exit was selected:
 *  ------- Writes Schedule to a File
 *  ------- Exits the program
 */
void sys_run() {
  Schedule *schedule = NULL;
  Process *on_cpu = NULL;
  Process *new_process = NULL;
  /* Manage the Trace file Reading */
  char trace_buffer[512]; 
  int trace_len = 512;
  int ret_code = 0;
  int do_exit = 0; /* Set by Trace function's exit command */

/* Stage 0) Calls Scheduler's Init Function to create the Data  */
  schedule = scheduler_init();
  if(!valid_schedule(schedule)) {
    printf("Error: Failed to create schedule properly.\n");
    exit(EXIT_FAILURE);
  }

  /* Initialize the Master OS Clock */
  clock_init(1);

  /* Run the System until Time to End */
  while(!do_exit) {
    printf(".=============\n");
    printf("| Starting Time: %2d\n", clock_get_time());
    printf("+------------\n");
/* Stage 1) Unload the Running Process from CPU and Return to Scheduler */
    if(on_cpu != NULL) {
      /* Call Scheduler's Add Function on Process */
      printf("| Returning Process (PID: %d) to Scheduler\n", on_cpu->pid);
      print_queue(on_cpu);
      printf("+------------\n");
      scheduler_add(schedule, on_cpu);
      on_cpu = NULL;
    }
    else {
      printf("| Nothing to Return to Scheduler\n");
      printf("+------------\n");
    }
/* Stage 2) Perform 1 action from the trace file  */
    ret_code = (fgets(trace_buffer, trace_len, fp_trace) != NULL);

    /* If exit (or EOF): Exits after Advance Time stage. */
    if(ret_code <= 0 || strstr(trace_buffer, "exit") != NULL) {
      printf("| Exit Selected\n");
      do_exit = 1;
      ret_code = 0;
    }
    /* If kill... */
    else if(strstr(trace_buffer, "kill") != NULL) {
      ret_code = parse_kill(schedule, trace_buffer);
    }
    /* If reap... */
    else if(strstr(trace_buffer, "reap") != NULL) {
      ret_code = parse_reap(schedule, trace_buffer);
    }
    /* If pass...*/
    else if(strstr(trace_buffer, "pass") != NULL) {
      printf("| Passing (No Action)\n");
      ret_code = 0;
    }
    /* If Create Process:
     *   Call Scheduler's Generate Function with Process Data
     *   Call Scheduler's Insert Function on Process
     */
    else {
      new_process = parse_command(trace_buffer, schedule);

      if(new_process != NULL) {
        printf("| Process Starting\n");
        print_queue(new_process);
        ret_code = scheduler_add(schedule, new_process);
      }
    }

    /* An action has been taken. Did it work? */
    if(ret_code != 0) {
      printf("Error: Failed to Parse the next Trace Action\n");
      exit(EXIT_FAILURE);
    }
      
    printf("+------------\n");
    printf("| Ready Queue:   %2d Processes\n", scheduler_count(schedule->ready_queue));
    print_queue(schedule->ready_queue->head);
    printf("| Stopped Queue: %2d Processes\n", scheduler_count(schedule->stopped_queue));
    print_queue(schedule->stopped_queue->head);
    printf("| Defunct Queue: %2d Processes\n", scheduler_count(schedule->defunct_queue));
    print_queue(schedule->defunct_queue->head);
    
/* Step 3) Select the Next Process to Run */
    on_cpu = scheduler_select(schedule);
    if(on_cpu != NULL) {
      on_cpu->next = NULL; /* Clean up the Remove */
    }

    printf("+------------\n");
    printf("| Selecting to Run on the CPU\n");
    print_queue(on_cpu);
    printf("\\=============\n\n");

/* Step 4) Advance Time (if not exiting) */
    if(do_exit == 0) {
      if(on_cpu != NULL) {
        on_cpu->time_remaining--;
        if(on_cpu->time_remaining <= 0) {
          on_cpu->time_remaining = 0;
//          on_cpu->flags &= 0x3F; // Zeroize upper bits (exit code)
          on_cpu->flags |= ((get_code(on_cpu->pid)& ~(0x1F << EXITCODE_BITS)));
        }
      }
      clock_advance_time();
    }
  }

  //dump_schedule(schedule, on_cpu);
  if(on_cpu != NULL) {
    free(on_cpu->command);
    free(on_cpu);
  }
  scheduler_free(schedule);
  free_trackers();

  printf("We're done!\n");
  return;
}

/* Prints a Queue
 */
static void print_queue(Process *queue) {
  Process *walker = NULL;
  char flags[9] = {0};
  if(queue == NULL) {
    printf("|\tNone\n");
  } else {
    for(walker = queue; walker != NULL; walker = walker->next) {
      load_flags(flags, walker->flags);
      printf("|\tPID: %3d Time Remaining: %2d Priority (Cur: %2d, Base: %d) Command: %s Flags:%s\n", 
             walker->pid, walker->time_remaining, walker->cur_priority,
             walker->base_priority, walker->command, flags);
    }
  }
  return;
}

/* If kill:
 *   If kill -STOP: 
 *     Call Scheduler's Stop Function on PID
 *   If kill -CONT: 
 *     Call Scheduler's Continue Function on PID
 */
static int parse_kill(Schedule *schedule, char *trace_buffer) {
  if(!valid_schedule(schedule) || trace_buffer == NULL) {
    return -1;
  }

  int ret_code = 0;
  int pid = 0;
  int do_stop = 0; /* Set by Trace function's kill command */
  int do_cont = 0; /* Set by Trace function's kill command */
  char *p_buf = NULL;

  p_buf = strtok(trace_buffer, " -"); /* Pop the kill - out of there */
  p_buf = strtok(NULL, " "); /* Position p_buf to SIG Code */
  if(strstr(p_buf, "STOP") != NULL) {
    do_stop = 1;
  }
  else if(strstr(p_buf, "CONT") != NULL) {
    do_cont = 1;
  }
  else {
    printf("Error: Cannot parse the Trace File.  kill misformatted\n");
    printf("Exiting.\n");
    exit(EXIT_FAILURE);
  }

  /* Now get the PID */
  p_buf = strtok(NULL, " "); /* Position p_buf to SIG Code */
  ret_code = sscanf(p_buf, "%d", &pid);
  if(ret_code != 1) {
    printf("Error: Cannot parse the Trace File.  kill misformatted\n");
    printf("Exiting.\n");
    exit(EXIT_FAILURE);
  }
  if(do_stop) {
    printf("| Sending STOP to Process (PID: %2d)\n", pid);
    ret_code = scheduler_stop(schedule, pid);
  }
  else if(do_cont) {
    printf("| Sending CONT to Process (PID: %2d)\n", pid);
    ret_code = scheduler_continue(schedule, pid);
  }
  else {
    ret_code = -1;
  }
  return ret_code;
}

/* If reap:
 *   Call Scheduler's Reap Function on PID
 */
static int parse_reap(Schedule *schedule, char *trace_buffer) {
  if(!valid_schedule(schedule) || trace_buffer == NULL) {
    return -1;
  }

  int exit_code = 0;
  int pid = 0;
  int ret = 0;
  char *p_buf = NULL;

  p_buf = strtok(trace_buffer, " "); /* Position p_buf to SIG Code */
  p_buf = strtok(NULL, " "); /* Position p_buf to PID */
  ret = sscanf(p_buf, "%d", &pid);
  if(ret != 1) {
    printf("Error: Cannot parse the Trace File.  kill misformatted\n");
    printf("Exiting.\n");
    exit(EXIT_FAILURE);
  }
  exit_code = scheduler_reap(schedule, pid);
  printf("| Reaping Process (PID: %2d). Exited with code: %d\n", pid, exit_code);

  return 0;
}

/* Looking for [sudo] commmand [args] \[pid,runtime,exitcode\]
 * Returns the process created or NULL on error
 */
static Process *parse_command(char *trace_buffer, Schedule *schedule) {
  int is_sudo = 0;
  int pid = 0;
  int run_time = 0;
  int priority = 0;
  int exit_code = 0;
  int count = 0;
  char *p_buf = NULL;
  char *command = NULL;
  Process *new_process = NULL;

  if(!valid_schedule(schedule) || trace_buffer == NULL) {
    return NULL;
  }

  if(strstr(trace_buffer, "sudo") != NULL) {
    is_sudo = 1;
    p_buf = strtok(trace_buffer, " "); /* Pops the sudo out */
    command = strtok(NULL, "[");
  }
  else {
    command = strtok(trace_buffer, "["); /* Pops out everything ahead of '[' */
  }
  p_buf = strtok(NULL, "]"); /* Pop the vals between [ ] */
  count = sscanf(p_buf, "%d,%d,%d,%d", &pid, &run_time, &priority, &exit_code);
  if(count != 4) {
    printf("Error: Could not parse the program to run in the trace file\n");
    exit(EXIT_FAILURE);
  }

  if(add(pid, exit_code) != 0) {
    printf("Error: Internal Tracker Error\n");
    printf("If you get this error, it's Prof. Andrea's bug, not yours.\n");
    printf("Please let him know.\n");
    exit(EXIT_FAILURE);
  }

  new_process = scheduler_generate(command, pid, priority, run_time, is_sudo);
  return new_process;
}

/* Local function to add pid:exitcode pairs to a queue */
static int add(int pid, int code) {
  Pid_Tracker *entry = malloc(sizeof(Pid_Tracker));
  memset(entry, '\0', sizeof(Pid_Tracker));
  if(entry == NULL) {
    return -1;
  }
  entry->pid = pid;
  entry->exit_code = code;

  if(pid_tracker == NULL) {
    pid_tracker = entry;
  }
  else {
    entry->next = pid_tracker;
    pid_tracker = entry;
  }
  return 0;
}

static void free_trackers() {
  Pid_Tracker *walker = pid_tracker;
  Pid_Tracker *reaper = NULL;
  while(walker) {
    reaper = walker;
    walker = walker->next;
    free(reaper);
  }
}
  
/* Local function to get pid:exitcode pairs from a queue */
static int get_code(int pid) {
  Pid_Tracker *head = pid_tracker;
  Pid_Tracker *walker = NULL;
  Pid_Tracker *reaper = NULL;
  int code = -1;
  if(head == NULL) {
    return -1;
  }
  
  if(head->pid == pid) {
    code = head->exit_code;
    reaper = head;
    pid_tracker = head->next;
    reaper->next = NULL;
    free(reaper);
  }
  else {
    for(walker = head; walker->next && walker->next->pid != pid; walker = walker->next)
      ;
    if(walker->next != NULL) {
      code = walker->next->exit_code;
      reaper = walker->next;
      walker->next = reaper->next;
      reaper->next = NULL;
      free(reaper);
    }
  }

  return code;
}

/* Local function to verify that the schedule is valid */
static int valid_schedule(Schedule *schedule) {
  if(schedule == NULL 
      || schedule->ready_queue == NULL 
      || schedule->stopped_queue == NULL
      || schedule->defunct_queue == NULL) {
    return 0;
  }
  return 1;
}

/* Dumps the Schedule to File */
/*
static void dump_schedule(Schedule *schedule, Process *p) {
  FILE *fp_output = fopen("out.dat", "w");

  if(!valid_schedule(schedule)) {
    fclose(fp_output);
    return;
  }
  
  fprintf(fp_output,   "End of Time %d\n", clock_get_time());
  dump_queue(fp_output, "Ready Queue:   ", schedule->ready_queue);
  dump_queue(fp_output, "Stopped Queue: ", schedule->stopped_queue);
  dump_queue(fp_output, "Defunct Queue: ", schedule->defunct_queue);
  fprintf(fp_output,   "On the CPU:   \n");
  if(p != NULL) {
    fprintf(fp_output, "| %d,0x%x,%d,%d,%s\n", p->pid, p->flags, p->time_remaining, p->time_last_run, p->command);
  }

  fclose(fp_output);
}
*/
/* Dumps a single queue to file */
/*
static void dump_queue(FILE *fp, char *command, Queue *ll) {
  fprintf(fp, "%s\n", command);
  Process *p = ll->head;
  int i = 0;

  while(p != NULL) {
    for(i = strlen(p->command)-1; isspace(p->command[i]); i--) {
      p->command[i] = '\0';
    }
    fprintf(fp, "| %d,0x%x,%d,%d,%s\n", p->pid, p->flags, p->time_remaining, p->time_last_run, p->command);
    p = p->next;
  }
}
*/
static void load_flags(char *ary, int flags) {
  flags >>= 26;
  sprintf(ary, "[%c%c%c%c%c%c]",
      flags&0x10?'C':' ',
      flags&0x8?'R':' ',
      flags&0x4?'T':' ',
      flags&0x2?'Z':' ',
      flags&0x1?'X':' ',
      flags&0x20?'S':' ');
  return;
}
