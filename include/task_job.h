#ifndef TASK_JOB_H
#define TASK_JOB_H

#include <pthread.h>
#include <stdio.h>
#include "min_heap.h"
#define num_levels 2
#define NUM_CORES 64

#define COLOR_RESET   "\x1b[0m"
#define COLOR_GREEN   "\x1b[32m" // Completions
#define COLOR_BLUE    "\x1b[34m" // Arrivals
#define COLOR_YELLOW  "\x1b[33m" // Preemptions
#define COLOR_RED     "\x1b[31m" // Mode Switches & Drops
#define COLOR_CYAN    "\x1b[36m" // System State Dump
#define COLOR_MAGENTA "\x1b[35m" // Offline Phase / Hyperperiod info

typedef struct taskDef
{
    int id;
    int level;
    int arrival_time;
    int period;
    int wcets[num_levels];
    int relative_deadline;
} TaskDef;

typedef struct task
{
    TaskDef* def;
    int next_arrival_time;
    double virtual_deadline;
    int job_count;
    bool active;
    int assigned_core;
} TaskState;

typedef struct job
{
    int id;
    TaskState* task;
    int arrival_time;  
    double absolute_deadline;
    int exec_time_remaining;
    int time_executed;
} Job;

typedef struct core
{
    int core_id;
    int local_time;
    int next_event_time;
    int k_boundary;
    double x_table[num_levels];
    Job* running_job;
    FILE* log_file;
    TaskState* core_tasks;
    int num_core_tasks;
    MinHeap* ready_queue;
} Core;

typedef struct cpu
{
    int num_cores;
    int global_time;
    int core_winner_event_type;
    int next_event_times[NUM_CORES];
    int next_event_type[NUM_CORES];
    pthread_barrier_t event_barrier;
    pthread_barrier_t time_barrier;
    Core* cores;
    int current_level;
    int num_tasks;
    TaskState* tasks;
} CPU;

#endif