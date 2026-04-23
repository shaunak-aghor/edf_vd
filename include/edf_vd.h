#ifndef EDF_VD_H
#define EDF_VD_H

#include "task_job.h"
#include <stdio.h>

// x_table must be an array of size num_levels.
// x_table[k-1] holds the x value computed for the transition at level boundary k.
// Entries default to 1.0 (no scaling). Only entries where a valid k was found are set.
bool edf_vd_preprocess(Task* task_set, int num_tasks, double* x_table, int* k_out);

void simulate_edf_vd(Task* tasks, int num_tasks, int k_boundary, double* x_table, FILE* log_file);


#endif