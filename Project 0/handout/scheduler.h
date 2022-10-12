/* Do Not Modify This File */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "structs.h"

/* Prototypes in the Scheduler API */
Schedule *scheduler_init();
int scheduler_add(Schedule *schedule, Process *process);
int scheduler_stop(Schedule *schedule, int pid);
int scheduler_continue(Schedule *schedule, int pid);
Process *scheduler_generate(char *command, int pid, int base_priority,
                            int cur_priority, int is_sudo);
int scheduler_reap(Schedule *schedule, int pid);
Process *scheduler_select(Schedule *schedule);
int scheduler_count(Queue *ll);
void scheduler_free(Schedule *schedule);

#endif /* SCHEDULER_H */
