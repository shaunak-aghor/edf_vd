#include "../include/common.h"
#include "../include/utils.h"
#include <limits.h>

int lcm(int num1, int num2)
{
    int copy_num1 = num1;
    int copy_num2 = num2;
    while(num2 != 0)
    {
        int temp = num2;
        num2 = num1%num2;
        num1 = temp;
    }
    
    return (copy_num1 * copy_num2)/(num1);
}

int calculate_hyperperiod(Task* task_set, int num_tasks)
{
    int _lcm = task_set[0].period;
    for(int i = 1; i < num_tasks; i++)
    {
        _lcm = lcm(_lcm, task_set[i].period);
    }
    
    return _lcm;
}

double get_utilization(int wcet, int period)
{
    return (double)wcet/period;
}

int get_next_arrival_time(Task* tasks, int num_tasks) {
    int min_arrival = INT_MAX;
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].next_arrival_time < min_arrival) {
            min_arrival = tasks[i].next_arrival_time;
        }
    }
    return min_arrival;
}

int get_next_completion_time(Job* running_job, int current_time) 
{
    
    if (running_job == NULL) return INT_MAX;
    
    return current_time + running_job->exec_time_remaining;
}

int get_next_mode_switch_time(Job* running_job, int current_level, int current_time) {
    
    if (running_job == NULL) return INT_MAX;
    
    // A mode switch is triggered if a job exceeds the wcet of the current system level.
    int current_wcet_budget = running_job->task->wcets[current_level - 1];
    
    
    int budget_remaining_for_level = current_wcet_budget - running_job->time_executed;
    
    
    // It only triggers if its randomly assigned total execution time is larger than the budget.
    if (budget_remaining_for_level > 0 && running_job->exec_time_remaining > budget_remaining_for_level) 
    {
        return current_time + budget_remaining_for_level;
    }
    
    // If it finishes before the budget runs out, or if it already exceeded it, no *future* event here.
    return INT_MAX; 
}

void handle_job_completion(Job** running_job_ptr, int current_time) 
{
    
    if (*running_job_ptr != NULL) 
    {
        
        
        printf(COLOR_GREEN "[Time %d] Job %d (Task %d) COMPLETED." COLOR_RESET "\n", 
               current_time, (*running_job_ptr)->id, (*running_job_ptr)->task->id);
        
        free(*running_job_ptr);
        
        
        *running_job_ptr = NULL; 
    }
}

void handle_job_arrival(Task* tasks, int num_tasks, int current_time, MinHeap* priority_queue) 
{
    for (int i = 0; i < num_tasks; i++) 
    {
        
        if (tasks[i].next_arrival_time == current_time) 
        {
            
            Job* new_job = (Job*)malloc(sizeof(Job));
            
            
            new_job->id = tasks[i].job_count++;
            new_job->task = &tasks[i];
            new_job->arrival_time = current_time; 
            
            
            int highest_wcet = tasks[i].wcets[tasks[i].level - 1];
            new_job->exec_time_remaining = (rand() % highest_wcet) + 1; 
            new_job->time_executed = 0;
            
            
            new_job->absolute_deadline = (double)current_time + tasks[i].virtual_deadline;
            
            
            heap_push(priority_queue, new_job, new_job->absolute_deadline);
            
            printf(COLOR_BLUE "[Time %d] Task %d arrived. Spawned Job %d,%d (Exec: %d, DL: %.2f)." COLOR_RESET "\n", 
                   current_time, tasks[i].id, tasks[i].id,new_job->id, new_job->exec_time_remaining, new_job->absolute_deadline);

            tasks[i].next_arrival_time += tasks[i].period;


        }
    }
}

void handle_mode_switch(int* current_level_ptr, int k_boundary, Job** running_job_ptr, MinHeap* priority_queue, int current_time, Task* tasks, int* num_tasks_ptr) 
{
    
    (*current_level_ptr)++;
   
    printf(COLOR_RED "\n[Time %d] *** BUDGET EXHAUSTED: SYSTEM LEVEL INCREASED TO %d ***" COLOR_RESET "\n", 
           current_time, *current_level_ptr);
    
    for (int i = 0; i < *num_tasks_ptr; ) 
    {
        if (tasks[i].level < *current_level_ptr) 
        {
            
            printf(COLOR_RED "[Time %d] Task %d dropped permanently from system." COLOR_RESET "\n", 
                   current_time, tasks[i].id);
            
            
            for (int j = i; j < *num_tasks_ptr - 1; j++) {
                tasks[j] = tasks[j + 1];
            }
            
            (*num_tasks_ptr)--; 
        } 
        else 
        {
           
            if (*current_level_ptr > k_boundary) {
                tasks[i].virtual_deadline = (double)tasks[i].relative_deadline; 
            }
            i++;
        }
    }

    
    if (*running_job_ptr != NULL && *current_level_ptr > k_boundary) 
    {
        // needs its deadline extended
        double old_deadline = (*running_job_ptr)->absolute_deadline;
        (*running_job_ptr)->absolute_deadline = (double)(*running_job_ptr)->arrival_time + (*running_job_ptr)->task->relative_deadline;
        printf(COLOR_YELLOW "[Time %d] Running Job %d (Task %d) Deadline extended: %.2f → %.2f" COLOR_RESET "\n",
               current_time, (*running_job_ptr)->id, (*running_job_ptr)->task->id,
               old_deadline, (*running_job_ptr)->absolute_deadline);
    }

  
    update_heap_for_mode_switch(priority_queue, *current_level_ptr, k_boundary);
    
    printf("\n");
}


void print_system_state(int current_time, int current_level, Job* running_job, MinHeap* queue) 
{
    printf(COLOR_CYAN "=== STATE @ t=%d | System Level: %d ===" COLOR_RESET "\n", current_time, current_level);
    
    // 1. Log the CPU State
    printf("  CPU   : ");
    if (running_job == NULL) {
        printf("[IDLE]\n");
    } else {
        int total_exec_time = running_job->time_executed + running_job->exec_time_remaining;
        printf("Job %d (Task %d) | Exec: %d/%d | Deadline: %.2f\n", 
               running_job->id, 
               running_job->task->id,
               running_job->time_executed, 
               total_exec_time,
               running_job->absolute_deadline);
    }

    // 2. Log the Ready Queue State
    printf("  QUEUE : ");
    if (heap_is_empty(queue)) {
        printf("[EMPTY]\n");
    } else {
        // We iterate through the internal array. 
        // Note: A Min-Heap array isn't perfectly sorted left-to-right, 
        // but it shows exactly what is waiting!
        for (int i = 0; i < queue->size; i++) {
            Job* q_job = queue->data[i].job;
            printf("[J%d_T%d DL:%.1f] ", q_job->id, q_job->task->id, q_job->absolute_deadline);
        }
        printf("\n");
    }
    printf(COLOR_CYAN "=======================================" COLOR_RESET "\n\n");
}