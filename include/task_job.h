#ifndef TASK_JOB_H
#define TASK_JOB_H

#define num_levels 2

#define COLOR_RESET   "\x1b[0m"
#define COLOR_GREEN   "\x1b[32m" // Completions
#define COLOR_BLUE    "\x1b[34m" // Arrivals
#define COLOR_YELLOW  "\x1b[33m" // Preemptions
#define COLOR_RED     "\x1b[31m" // Mode Switches & Drops
#define COLOR_CYAN    "\x1b[36m" // System State Dump
#define COLOR_MAGENTA "\x1b[35m" // Offline Phase / Hyperperiod info


typedef struct task
{
    int id;
    int level;
    int arrival_time;
    int period;
    int wcets[num_levels];
    int relative_deadline;
    int next_arrival_time;
    double virtual_deadline;
    int job_count;
} Task;

typedef struct job
{
    int id;
    Task* task;
    int arrival_time;  
    double absolute_deadline;
    int exec_time_remaining;
    int time_executed;
} Job;


#endif

