#include "../include/common.h"
#include "../include/utils.h"
#include <limits.h>
#include <stdarg.h>
#include <string.h>

// --- Logging ---

void log_write(FILE* log_file, int timestamp, const char* fmt, ...)
{
    if (log_file == NULL) return;

    // Print timestamp prefix
    fprintf(log_file, "[t=%d] ", timestamp);

    // Print formatted message
    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);

    // Ensure newline
    if (fmt[strlen(fmt) - 1] != '\n')
        fprintf(log_file, "\n");

    fflush(log_file);
}

// --- Math helpers ---

int lcm(int num1, int num2)
{
    int copy_num1 = num1;
    int copy_num2 = num2;
    while (num2 != 0)
    {
        int temp = num2;
        num2 = num1 % num2;
        num1 = temp;
    }
    return (copy_num1 * copy_num2) / num1;
}

int calculate_hyperperiod(Task* task_set, int num_tasks)
{
    int _lcm = task_set[0].period;
    for (int i = 1; i < num_tasks; i++)
        _lcm = lcm(_lcm, task_set[i].period);
    return _lcm;
}

double get_utilization(int wcet, int period)
{
    return (double)wcet / period;
}


// --- Timing functions ---

int get_next_arrival_time(Task* tasks, int num_tasks)
{
    int min_arrival = INT_MAX;
    for (int i = 0; i < num_tasks; i++)
    {
        if (!tasks[i].active) continue;    // skip dropped tasks
        if (tasks[i].next_arrival_time < min_arrival)
            min_arrival = tasks[i].next_arrival_time;
    }
    return min_arrival;
}

int get_next_completion_time(Job* running_job, int current_time)
{
    if (running_job == NULL) return INT_MAX;
    return current_time + running_job->exec_time_remaining;
}

int get_next_mode_switch_time(Job* running_job, int current_level, int current_time)
{
    if (running_job == NULL) return INT_MAX;

    // A mode switch fires when the running job burns through its budget
    // for the current criticality level.
    int current_wcet_budget     = running_job->task->wcets[current_level - 1];
    int budget_remaining         = current_wcet_budget - running_job->time_executed;

    // Only fires if the job would actually exceed the budget before finishing.
    if (budget_remaining > 0 && running_job->exec_time_remaining > budget_remaining)
        return current_time + budget_remaining;

    return INT_MAX;
}


// --- Event handlers ---

void handle_job_completion(Job** running_job_ptr, int current_time, FILE* log_file)
{
    if (*running_job_ptr != NULL)
    {
        log_write(log_file, current_time, 
                  "Job %d (Task %d) COMPLETED.",
                  (*running_job_ptr)->id, (*running_job_ptr)->task->id);

        free(*running_job_ptr);
        *running_job_ptr = NULL;
    }
}

void handle_mode_switch(int* current_level_ptr, int k_boundary, Job** running_job_ptr,
                        MinHeap* priority_queue, int current_time,
                        Task* tasks, int num_tasks, double* x_table, FILE* log_file)
{
    (*current_level_ptr)++;
    int new_level = *current_level_ptr;

    log_write(log_file, current_time,
              "*** BUDGET EXHAUSTED: SYSTEM LEVEL INCREASED TO %d ***", new_level);

    // Look up the x value for the new level.
    // x_table[k-1] was set during preprocessing for boundary k.
    // At the highest level (num_levels), this defaults to 1.0 (set in preprocess).
    double new_x = x_table[new_level - 1];

    log_write(log_file, current_time,
              "Applying x = %.4f for level %d virtual deadlines.", new_x, new_level);

    // Update all active tasks.
    // LO tasks (level < new_level) are permanently dropped.
    // HI tasks get their virtual deadlines recalculated with the new x.
    for (int i = 0; i < num_tasks; i++)
    {
        if (!tasks[i].active) continue;

        if (tasks[i].level < new_level)
        {
            tasks[i].active = false;    // mark dropped
            log_write(log_file, current_time,
                      "Task %d (level %d) dropped permanently.",
                      tasks[i].id, tasks[i].level);
        }
        else
        {
            // Recalculate virtual deadline with the correct x for this level.
            tasks[i].virtual_deadline = new_x * tasks[i].period;
        }
    }

    // Handle the currently running job.
    if (*running_job_ptr != NULL)
    {
        if ((*running_job_ptr)->task->level < new_level)
        {
            // The running job belongs to a now-dropped task. Kill it immediately.
            log_write(log_file, current_time,
                      "Running Job %d (Task %d) killed — task dropped.",
                      (*running_job_ptr)->id, (*running_job_ptr)->task->id);
            free(*running_job_ptr);
            *running_job_ptr = NULL;
        }
        else if (new_level > k_boundary)
        {
            // HI task above k_boundary: extend deadline using the new virtual deadline.
            double old_dl = (*running_job_ptr)->absolute_deadline;
            (*running_job_ptr)->absolute_deadline =
                (double)(*running_job_ptr)->arrival_time + (*running_job_ptr)->task->virtual_deadline;

            log_write(log_file, current_time,
                      "Running Job %d (Task %d) deadline updated: %.2f -> %.2f",
                      (*running_job_ptr)->id, (*running_job_ptr)->task->id,
                      old_dl, (*running_job_ptr)->absolute_deadline);
        }
    }

    // Update the ready queue: drop jobs from dropped tasks, reprioritize survivors.
    update_heap_for_mode_switch(priority_queue, new_level, k_boundary);
}

void handle_job_arrival(Task* tasks, int num_tasks, int current_time, MinHeap* priority_queue, FILE* log_file)
{
    for (int i = 0; i < num_tasks; i++)
    {
        if (!tasks[i].active) continue;    // skip dropped tasks

        if (tasks[i].next_arrival_time == current_time)
        {
            Job* new_job = (Job*)malloc(sizeof(Job));

            new_job->id                  = tasks[i].job_count++;
            new_job->task                = &tasks[i];
            new_job->arrival_time        = current_time;
            new_job->time_executed       = 0;

            // Randomised execution time between 1 and the task's highest-level WCET.
            int highest_wcet             = tasks[i].wcets[tasks[i].level - 1];
            new_job->exec_time_remaining = (rand() % highest_wcet) + 1;

            // Absolute deadline uses the virtual deadline assigned during preprocessing
            // (or updated on the last mode switch).
            new_job->absolute_deadline   = (double)current_time + tasks[i].virtual_deadline;

            heap_push(priority_queue, new_job, new_job->absolute_deadline);

            log_write(log_file, current_time,
                      "Task %d arrived. Spawned Job %d (Exec: %d, DL: %.2f).",
                      tasks[i].id, new_job->id, new_job->exec_time_remaining, new_job->absolute_deadline);

            tasks[i].next_arrival_time += tasks[i].period;
        }
    }
}


// --- Logging ---

void print_system_state(int current_time, int current_level, Job* running_job, MinHeap* queue, FILE* log_file)
{
    if (log_file == NULL) return;

    fprintf(log_file, "[t=%d] === STATE @ t=%d | System Level: %d ===\n",
           current_time, current_time, current_level);

    fprintf(log_file, "[t=%d]   CPU   : ", current_time);
    if (running_job == NULL)
    {
        fprintf(log_file, "[IDLE]\n");
    }
    else
    {
        int total = running_job->time_executed + running_job->exec_time_remaining;
        fprintf(log_file, "Job %d (Task %d) | Exec: %d/%d | Deadline: %.2f\n",
               running_job->id, running_job->task->id,
               running_job->time_executed, total,
               running_job->absolute_deadline);
    }

    fprintf(log_file, "[t=%d]   QUEUE : ", current_time);
    if (heap_is_empty(queue))
    {
        fprintf(log_file, "[EMPTY]\n");
    }
    else
    {
        for (int i = 0; i < queue->size; i++)
        {
            Job* j = queue->data[i].job;
            fprintf(log_file, "[J%d_T%d DL:%.1f] ", j->id, j->task->id, j->absolute_deadline);
        }
        fprintf(log_file, "\n");
    }

    fprintf(log_file, "[t=%d] =======================================\n\n", current_time);
    fflush(log_file);
}