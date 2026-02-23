#include "../include/common.h"
#include "../include/min_heap.h"
#include "../include/utils.h"
#define EPSILON 1e-9

// Helper: minimum of three ints
static int min3(int a, int b, int c) {
    int m = a < b ? a : b;
    return m < c ? m : c;
}


//offline phase
bool edf_vd_preprocess(Task* task_set, int num_tasks, double* x_out, int* k_out)
{

    //check if all jobs running at it highest wcet at highest level can be scheduled 
    double sum_Ul_l = 0.0;
    for(int i = 0; i < num_tasks; i++)
    {
        int cur_level = task_set[i].level;
        sum_Ul_l += get_utilization(task_set[i].wcets[cur_level - 1], task_set[i].period);
    }

    //if the total utilization is less than 1.00, set virtual deadlines = relative deadlines, since we
    //are considering implicit deadlines (relative_deadline = period) we will set it to period.
    if(sum_Ul_l <= 1.0 + EPSILON)
    {
        *k_out = 0;   //no specific mode switch boundary is forced
        *x_out = 1.0; // No scaling
        for(int i = 0; i < num_tasks; i++)
        {
            task_set[i].virtual_deadline = task_set[i].period;
        }

        return true;
    }

    //else we need to find a k and a x
    else
    {
        for(int k = 1; k < num_levels; k++)
        {
            double u_lo_lo = 0.0; // Sum of U_l(l) for l <= k
            double u_hi_k  = 0.0; // Sum of U_l(k) for l > k 
            double u_hi_hi = 0.0; // Sum of U_l(l) for l > k

            for(int j = 0; j < num_tasks; j++)
            {
                Task task = task_set[j];
                int cur_level = task.level;

                if(cur_level <= k)
                {
                    u_lo_lo += get_utilization(task.wcets[cur_level - 1], task.period);
                }
                else
                {
                    u_hi_k += get_utilization(task.wcets[k - 1], task.period);
                    u_hi_hi += get_utilization(task.wcets[cur_level - 1], task.period);
                }

            }

            if(u_lo_lo >= 1.0)
            {
                return false;
            }

            double x_min = (u_hi_k)/(1 - u_lo_lo);
            double x_max = (1 - u_hi_hi)/(u_lo_lo);

            if((x_min <= x_max + EPSILON))
            {
                *x_out = x_min; // Smallest x is safest/tightest
                *k_out = k;
            
                // Assign Virtual Deadlines
                for(int i = 0; i < num_tasks; i++)
                {
                    if (task_set[i].level <= k) 
                    {
                        task_set[i].virtual_deadline = task_set[i].period;
                    }   
                    else 
                    {
                        task_set[i].virtual_deadline = (x_min * task_set[i].period);
                    }
                }
                return true;
            }
            
        }
        
        return false;
    }

    
}


//runtime phase
void simulate_edf_vd(Task* tasks, int num_tasks, int k_boundary) 
{
    
    int current_time = 0;
    int current_level = 1;
    Job* running_job = NULL;
    MinHeap* priority_queue = heap_init(num_tasks);

    int hyperperiod = calculate_hyperperiod(tasks, num_tasks);
    
    printf("Starting EDF-VD Simulation for Hyperperiod: %d\n", hyperperiod);

    
    while (current_time < hyperperiod) 
    {
        
        int t_next_arrival = get_next_arrival_time(tasks, num_tasks);
        int t_next_completion = get_next_completion_time(running_job, current_time);
        int t_next_mode_switch = get_next_mode_switch_time(running_job, current_level, current_time);

        
        int next_event_time = min3(t_next_arrival, t_next_completion, t_next_mode_switch);

        
        int elapsed = next_event_time - current_time;
        current_time = next_event_time;
        
        
        if (running_job != NULL) {
            running_job->time_executed += elapsed;
            running_job->exec_time_remaining -= elapsed;
        }

        
        if (current_time == t_next_completion) {
            handle_job_completion(&running_job, current_time);
        }

        if (current_time == t_next_mode_switch) {
            handle_mode_switch(&current_level, k_boundary, &running_job, priority_queue, current_time, tasks, &num_tasks);
        }

        if (current_time == t_next_arrival) {
            handle_job_arrival(tasks, num_tasks, current_time, priority_queue);
        }

        // Dispatcher: if no running job, take from queue
        if (running_job == NULL && !heap_is_empty(priority_queue)) 
        {
            running_job = heap_pop(priority_queue);
            if (running_job != NULL) {
                printf(COLOR_BLUE "[Time %d] CPU DISPATCH: Job %d (Task %d) starts execution." COLOR_RESET "\n",
                       current_time, running_job->id, running_job->task->id);
            }
        } else if (running_job != NULL && !heap_is_empty(priority_queue)) {
            // Preemption: if a queued job has earlier deadline (smaller priority)
            Job* peek = heap_peek(priority_queue);
            if (peek != NULL && peek->absolute_deadline + EPSILON < running_job->absolute_deadline) {
                // preempt running job
                printf(COLOR_YELLOW "[Time %d] PREEMPTION: Job %d (Task %d) preempted by Job %d (Task %d)!" COLOR_RESET "\n", 
                       current_time, running_job->id, running_job->task->id, peek->id, peek->task->id);
                heap_push(priority_queue, running_job, running_job->absolute_deadline);
                running_job = heap_pop(priority_queue);
                printf(COLOR_BLUE "[Time %d] CPU DISPATCH: Job %d (Task %d) starts execution (post-preemption)." COLOR_RESET "\n",
                       current_time, running_job->id, running_job->task->id);
            }
        }
    }
    
    // Cleanup: free any remaining jobs in queue
    while (!heap_is_empty(priority_queue)) {
        Job* j = heap_pop(priority_queue);
        if (j) free(j);
    }

    heap_destroy(priority_queue);

    printf("Simulation completed successfully.\n");
}