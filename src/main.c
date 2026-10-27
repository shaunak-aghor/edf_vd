#include "../include/common.h"
#include "../include/partition.h"
#include <limits.h>

static CPU* g_cpu = NULL;

typedef struct
{
    Core* core;
    int hyperperiod;
} CoreWorkerArgs;

static void* core_worker(void* arg)
{
    CoreWorkerArgs* args = arg;

    simulate_edf_vd(g_cpu, args->core, args->hyperperiod);
    return NULL;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("usage: %s <taskset.txt> [num_cores]\n", argv[0]);
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

    TaskDef   defs[num_tasks];
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

        defs[i].id                = i;
        defs[i].level             = level;
        defs[i].arrival_time      = arrival_time;
        defs[i].period            = period;
        defs[i].relative_deadline = relative_deadline;
        memcpy(defs[i].wcets, wcets, num_levels * sizeof(int));

        tasks[i].def               = &defs[i];
        tasks[i].next_arrival_time = arrival_time;
        tasks[i].job_count         = 0;
        tasks[i].virtual_deadline  = 0.0;
        tasks[i].active            = true;
        tasks[i].assigned_core     = -1;
    }

    fclose(task_file);

    printf("Read %d tasks successfully.\n", num_tasks);

    int num_cores = argc >= 3 ? atoi(argv[2]) : 1;
    if (num_cores <= 0 || num_cores > NUM_CORES)
    {
        printf("[ERROR] Number of cores must be between 1 and %d.\n", NUM_CORES);
        return -4;
    }

    if (!partition_tasks(tasks, num_tasks, num_cores))
    {
        printf("[ERROR] Could not partition tasks across %d cores.\n", num_cores);
        return -5;
    }

    int core_task_counts[num_cores];
    for (int core = 0; core < num_cores; core++)
        core_task_counts[core] = 0;

    for (int task = 0; task < num_tasks; task++)
        core_task_counts[tasks[task].assigned_core]++;

    printf("\n" COLOR_MAGENTA "--- STARTING EDF-VD RUNTIME SIMULATION ---" COLOR_RESET "\n\n");

    CPU cpu;
    Core* cores = calloc(num_cores, sizeof(Core));
    if (cores == NULL)
    {
        printf("[ERROR] Could not allocate cores.\n");
        return -6;
    }

    cpu.num_cores      = num_cores;
    cpu.global_time    = 0;
    cpu.cores          = cores;
    cpu.current_level  = 1;
    cpu.tasks          = tasks;
    cpu.num_tasks      = num_tasks;

    for (int i = 0; i < num_cores; i++)
    {
        cores[i].core_id     = i;
        cores[i].local_time  = 0;
        cores[i].next_event_time = INT_MAX;
        cores[i].k_boundary = -1;
        cores[i].running_job = NULL;
        cores[i].num_core_tasks = core_task_counts[i];
        cores[i].core_tasks = calloc(core_task_counts[i] > 0 ? core_task_counts[i] : 1,
                                     sizeof(TaskState));

        char log_path[64];
        snprintf(log_path, sizeof(log_path), "logs/core_%d.log", i);
        cores[i].log_file = fopen(log_path, "w");

        if (cores[i].core_tasks == NULL || cores[i].log_file == NULL)
        {
            printf("[ERROR] Could not initialize core %d.\n", i);
            return -7;
        }

        int local_task = 0;
        for (int task = 0; task < num_tasks; task++)
        {
            if (tasks[task].assigned_core == i)
                cores[i].core_tasks[local_task++] = tasks[task];
        }

        if (cores[i].num_core_tasks > 0 && !edf_vd_preprocess(cores[i].core_tasks, cores[i].num_core_tasks, cores[i].x_table, &cores[i].k_boundary))
        {
            printf("[RESULT] Core %d task partition is NOT schedulable under EDF-VD.\n", i);
            return -3;
        }

        printf(COLOR_MAGENTA "Core %d preprocessing successful.\n" COLOR_RESET,
               i);
        printf("  k_boundary = %d\n", cores[i].k_boundary);
        for (int level = 0; level < num_levels; level++)
            printf("  x_table[%d] = %.4f\n", level, cores[i].x_table[level]);
    }

    for (int i = 0; i < num_cores; i++)
        cpu.next_event_times[i] = cores[i].next_event_time;

    srand(42);
    g_cpu = &cpu;

    int max_hyperperiod = 0;
    for (int i = 0; i < num_cores; i++)
    {
        if (cores[i].num_core_tasks == 0)
            continue;

        int cur_hyperperiod = calculate_hyperperiod(cores[i].core_tasks, cores[i].num_core_tasks);

        if(cur_hyperperiod > max_hyperperiod)
        {
            max_hyperperiod = cur_hyperperiod;
        }
    }

    pthread_barrier_init(&cpu.event_barrier, NULL, num_cores);

    pthread_barrier_init(&cpu.time_barrier, NULL, num_cores);

    pthread_t threads[num_cores];
    CoreWorkerArgs worker_args[num_cores];

    for (int i = 0; i < num_cores; i++)
    {
        worker_args[i].core = &cores[i];
        worker_args[i].hyperperiod = max_hyperperiod;
        pthread_create(&threads[i], NULL, core_worker, &worker_args[i]);
    }

    for (int i = 0; i < num_cores; i++)
        pthread_join(threads[i], NULL);

    pthread_barrier_destroy(&cpu.time_barrier);
    pthread_barrier_destroy(&cpu.event_barrier);


    for (int i = 0; i < num_cores; i++)
    {
        fclose(cores[i].log_file);
        free(cores[i].core_tasks);
    }
    free(cores);

    printf("[INFO] Simulation logs written to logs/core_<n>.log\n");

    return 0;
}