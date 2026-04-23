#include "../include/common.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("usage: %s <taskset.txt>\n", argv[0]);
        return -1;
    }

    FILE* task_file = fopen(argv[1], "r");
    if (task_file == NULL)
    {
        printf("[ERROR] Could not open %s\n", argv[1]);
        return -2;
    }

    int num_tasks = 0;
    fscanf(task_file, "%d", &num_tasks);
    
    TaskDef  defs[num_tasks];
    TaskState tasks[num_tasks];

    for (int i = 0; i < num_tasks; i++)
    {
        int arrival_time, level, period, relative_deadline;
        int wcets[num_levels];

        fscanf(task_file, "%d ", &arrival_time);
        fscanf(task_file, "%d ", &level);

        for (int j = 0; j < num_levels; j++)
            fscanf(task_file, "%d ", &wcets[j]);

        fscanf(task_file, "%d %d\n", &period, &relative_deadline);

        defs[i].id                 = i;
        tasks[i].job_count          = 0;
        defs[i].level              = level;
        defs[i].arrival_time       = arrival_time;
        tasks[i].next_arrival_time  = arrival_time;
        defs[i].period             = period;
        defs[i].relative_deadline  = relative_deadline;
        tasks[i].active             = true;   // all tasks start active
        tasks[i].def = &defs[i];
        memcpy(defs[i].wcets, wcets, num_levels * sizeof(int));
    }

    fclose(task_file);

    printf("Read %d tasks successfully.\n", num_tasks);

    // x_table[k-1] holds the x_min value computed for level boundary k.
    // Defaults to 1.0 for all entries; edf_vd_preprocess fills in the relevant entry.
    double x_table[num_levels];
    int k_result = -1;

    if (edf_vd_preprocess(tasks, num_tasks, x_table, &k_result))
    {
        printf(COLOR_MAGENTA "Pre-processing successful.\n" COLOR_RESET);
        printf("  k_boundary = %d\n", k_result);
        for (int i = 0; i < num_levels; i++)
            printf("  x_table[%d] = %.4f\n", i, x_table[i]);
        printf("\n");

        for (int i = 0; i < num_tasks; i++)
            printf("  Task %d | level=%d | period=%d | wcets=[%d,%d] | virtual_deadline=%.4f\n",
                   defs[i].id, defs[i].level, defs[i].period,
                   defs[i].wcets[0], defs[i].wcets[1],
                   tasks[i].virtual_deadline);

        printf("\n" COLOR_MAGENTA "--- STARTING EDF-VD RUNTIME SIMULATION ---" COLOR_RESET "\n\n");

        // Open log file for this core (core_0.log for now, single-core)
        FILE* log_file = fopen("logs/core_0.log", "w");
        if (log_file == NULL)
        {
            printf("[ERROR] Could not open logs/core_0.log for writing\n");
            return -3;
        }

        srand(42);  // fixed seed for reproducible runs
        simulate_edf_vd(tasks, num_tasks, k_result, x_table, log_file);

        fclose(log_file);
        printf("[INFO] Simulation log written to logs/core_0.log\n");
    }
    else
    {
        printf("[RESULT] Task set is NOT schedulable under EDF-VD.\n");
    }

    return 0;
}