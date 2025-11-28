#ifndef SCHEDULER_DEMO_H
#define SCHEDULER_DEMO_H

#include "../stdtools.h"

/* Demo initialization and execution */
int scheduler_demo_init(void);
int scheduler_demo_run(void);

/* Task creation and management */
int demo_create_tasks(void);

/* Demonstration functions */
void demo_simulate_operation(void);
void demo_display_statistics(void);

#endif /* SCHEDULER_DEMO_H */