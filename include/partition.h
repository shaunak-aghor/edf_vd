#ifndef PARTITION_H
#define PARTITION_H

#include "task_job.h"

// Assign tasks to cores in input order using round-robin distribution.
bool partition_tasks(TaskState* tasks, int num_tasks, int num_cores);

#endif