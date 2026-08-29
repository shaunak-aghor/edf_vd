#include "../include/partition.h"

bool partition_tasks(TaskState* tasks, int num_tasks, int num_cores)
{
	if (tasks == NULL || num_tasks < 0 || num_cores <= 0)
		return false;

	for (int task = 0; task < num_tasks; task++)
		tasks[task].assigned_core = task % num_cores;

	return true;
}
