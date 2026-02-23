#ifndef EDF_VD_H
#define EDF_VD_H


#include "task_job.h"

bool edf_vd_preprocess(Task* task_set, int num_tasks, double* x_out, int* k_out);

void simulate_edf_vd(Task* tasks, int num_tasks, int k_boundary);


#endif