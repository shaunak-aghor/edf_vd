#include "../include/common.h"

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        printf("usage %s <filename.txt>\n",argv[0]);
        return -1;
    }

    FILE* task_file = fopen(argv[1],"r");
    int num_tasks = 0;

    if(task_file == NULL)
    {
        printf("[ERROR]: Error opening %s\n",argv[1]);
        return -2;
    }

    fscanf(task_file,"%d",&num_tasks);
    Task tasks[num_tasks];


    for(int i = 0; i < num_tasks; i++)
    {
        int arrival_time;
        int level;
        int period;
        int wcets[num_levels];
        int relative_deadline;
        fscanf(task_file,"%d ", &arrival_time);
        fscanf(task_file,"%d ", &level);
        
        for(int j = 0; j < num_levels; j++)
        {
            fscanf(task_file,"%d ", &wcets[j]);
        }

        fscanf(task_file,"%d %d\n", &period, &relative_deadline);

        tasks[i].id = i;
        tasks[i].job_count = 0;
        tasks[i].level = level;
        tasks[i].arrival_time = arrival_time;
        tasks[i].next_arrival_time = arrival_time;
        memcpy(tasks[i].wcets,wcets,num_levels * sizeof(int));
        tasks[i].period = period;
        tasks[i].relative_deadline = relative_deadline;
        
    }

    fclose(task_file);
    
    printf("Read %d tasks successfully.\n", num_tasks);

    int k_result = -1;       
    double x_result = -1.0;

    if (edf_vd_preprocess(tasks, num_tasks, &x_result, &k_result)) 
    {
        printf("Pre-processing Successful!\n");
        printf("Found k = %d, x = %.4f\n", k_result, x_result);
        
        // Print virtual deadlines to verify
        for(int i=0; i<num_tasks; i++) {
             printf("Task %d: Period %d -> VD %.4f\n", 
                    i, tasks[i].period, tasks[i].virtual_deadline);
        }

        printf("\n" COLOR_MAGENTA "--- STARTING EDF-VD RUNTIME SIMULATION ---" COLOR_RESET "\n");
        
        
        simulate_edf_vd(tasks, num_tasks, k_result);
    } 
    else 
    {
        printf("Task set is NOT schedulable.\n");
    }


    return 0;
}