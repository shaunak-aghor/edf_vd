#ifndef UTILS_H
#define UTILS_H

#include "min_heap.h"

//helper functions
double get_utilization(int wcet, int period);
int calculate_hyperperiod(Task* task_set, int num_tasks);


//timing functions
int get_next_arrival_time(Task* tasks, int num_tasks);
int get_next_completion_time(Job* running_job, int current_time);
int get_next_mode_switch_time(Job* running_job, int current_level, int current_time);

//event handlers
void handle_job_completion(Job** running_job_ptr, int current_time);
void handle_mode_switch(int* current_level_ptr, int k_boundary, Job** running_job_ptr, MinHeap* priority_queue, int current_time, Task* tasks, int* num_tasks_ptr);
void handle_job_arrival(Task* tasks, int num_tasks, int current_time, MinHeap* priority_queue);

//logging
void print_system_state(int current_time, int current_level, Job* running_job, MinHeap* queue);

#endif