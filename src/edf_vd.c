#include "../include/common.h"
#include "../include/min_heap.h"
#include "../include/utils.h"
#include <limits.h>

#define EPSILON 1e-9

// Helper: minimum of three ints
static int min3(int a, int b, int c) {
    int m = a < b ? a : b;
    return m < c ? m : c;
}


// --- Offline Phase ---

bool edf_vd_preprocess(TaskState* task_set, int num_tasks, double* x_table, int* k_out)
{
    // Default all x values to 1.0 (no scaling). Only overwritten if a valid k is found.
    for (int i = 0; i < num_levels; i++)
        x_table[i] = 1.0;

    // Check if all tasks running at their highest wcet at their highest level
    // are schedulable as-is.
    double sum_Ul_l = 0.0;
    for (int i = 0; i < num_tasks; i++)
    {
        int cur_level = task_set[i].def->level;
        sum_Ul_l += get_utilization(task_set[i].def->wcets[cur_level - 1], task_set[i].def->period);
    }

    // If total utilization <= 1.0, no virtual deadline scaling is needed.
    // Set virtual deadlines equal to periods (implicit deadlines).
    if (sum_Ul_l <= 1.0 + EPSILON)
    {
        *k_out = 0;
        // x_table already all 1.0 from initialization above
        for (int i = 0; i < num_tasks; i++)
            task_set[i].virtual_deadline = task_set[i].def->period;

        return true;
    }

    // Otherwise find a valid k and corresponding x_min.
    for (int k = 1; k < num_levels; k++)
    {
        double u_lo_lo = 0.0; // sum of U_l(l) for tasks with level <= k  (LO tasks at their own wcet)
        double u_hi_k  = 0.0; // sum of U_l(k) for tasks with level >  k  (HI tasks at LO wcet)
        double u_hi_hi = 0.0; // sum of U_l(l) for tasks with level >  k  (HI tasks at their own wcet)

        for (int j = 0; j < num_tasks; j++)
        {
            TaskState task = task_set[j];
            int cur_level = task.def->level;

            if (cur_level <= k)
            {
                u_lo_lo += get_utilization(task.def->wcets[cur_level - 1], task.def->period);
            }
            else
            {
                u_hi_k  += get_utilization(task.def->wcets[k - 1], task.def->period);
                u_hi_hi += get_utilization(task.def->wcets[cur_level - 1], task.def->period);
            }
        }

        if (u_lo_lo >= 1.0)
            return false;

        double x_min = u_hi_k  / (1.0 - u_lo_lo);
        double x_max = (1.0 - u_hi_hi) / u_lo_lo;

        if (x_min <= x_max + EPSILON)
        {
            // Store x_min for this level boundary.
            // x_table[k-1] is the x used when the system is operating at level k
            // (i.e. after a switch from level k-1 to k, or simply in LO mode for k=1).
            x_table[k - 1] = x_min;
            *k_out = k;

            // Assign virtual deadlines based on which side of k each task sits.
            for (int i = 0; i < num_tasks; i++)
            {
                if (task_set[i].def->level <= k)
                    task_set[i].virtual_deadline = task_set[i].def->period;
                else
                    task_set[i].virtual_deadline = x_min * task_set[i].def->period;
            }

            return true;
        }
    }

    return false;
}


// --- Runtime Phase ---

void simulate_edf_vd(TaskState* tasks, int num_tasks, int k_boundary, double* x_table, FILE* log_file)
{
    int current_time  = 0;
    int current_level = 1;
    Job* running_job  = NULL;

    // Initial capacity of 64 — heap auto-resizes.
    MinHeap* priority_queue = heap_init(64);

    int hyperperiod = calculate_hyperperiod(tasks, num_tasks);

    while (current_time < hyperperiod)
    {
        int t_next_arrival     = get_next_arrival_time(tasks, num_tasks);
        int t_next_completion  = get_next_completion_time(running_job, current_time);
        int t_next_mode_switch = get_next_mode_switch_time(running_job, current_level, current_time);

        int next_event_time = min3(t_next_arrival, t_next_completion, t_next_mode_switch);

        //if no event is scheduled and nothing is running, something is wrong.
        if (next_event_time == INT_MAX)
        {
            fprintf(stderr, "[FATAL] No future events and CPU idle at t=%d. "
                            "Possible deadlock or input bug.\n", current_time);
            break;
        }

        // Advance time and account for execution on the running job.
        int elapsed   = next_event_time - current_time;
        current_time  = next_event_time;

        if (running_job != NULL)
        {
            running_job->time_executed        += elapsed;
            running_job->exec_time_remaining  -= elapsed;
        }

        // Handle events in order: completion before mode-switch before arrival.
        if (current_time == t_next_completion)
            handle_job_completion(&running_job, current_time, log_file);

        if (current_time == t_next_mode_switch)
            handle_mode_switch(&current_level, k_boundary, &running_job,
                priority_queue, current_time, tasks, num_tasks, x_table, log_file);

        if (current_time == t_next_arrival)
            handle_job_arrival(tasks, num_tasks, current_time, priority_queue, log_file);

        // Dispatcher: pick from queue if CPU is free.
        if (running_job == NULL && !heap_is_empty(priority_queue))
        {
            running_job = heap_pop(priority_queue);
            if (running_job != NULL)
                log_write(log_file, current_time,
                         "CPU DISPATCH: Job %d (Task %d) starts execution.",
                         running_job->id, running_job->task->def->id);
        }
        
        // Preemption check
        else if (running_job != NULL && !heap_is_empty(priority_queue))
        {
            Job* peek = heap_peek(priority_queue);
            if (peek != NULL && peek->absolute_deadline + EPSILON < running_job->absolute_deadline)
            {
                log_write(log_file, current_time,
                         "PREEMPTION: Job %d (Task %d) preempted by Job %d (Task %d)!",
                         running_job->id, running_job->task->def->id,
                         peek->id,        peek->task->def->id);

                heap_push(priority_queue, running_job, running_job->absolute_deadline);
                running_job = heap_pop(priority_queue);

                log_write(log_file, current_time,
                         "CPU DISPATCH: Job %d (Task %d) starts execution (post-preemption).",
                         running_job->id, running_job->task->def->id);
            }
        }
    }

    // Cleanup: free any jobs still in the queue at end of hyperperiod.
    while (!heap_is_empty(priority_queue))
    {
        Job* j = heap_pop(priority_queue);
        if (j) free(j);
    }
    if (running_job) free(running_job);

    heap_destroy(priority_queue);

    log_write(log_file, current_time,
              "Simulation completed at t=%d (hyperperiod=%d).",
              current_time, hyperperiod);
}