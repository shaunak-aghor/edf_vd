#include "../include/common.h"

static CPU* g_cpu = NULL;

static void* core_worker(void* arg)
{
    int core_id = *(int*)arg;
    Core* core = &g_cpu->cores[core_id];
    simulate_edf_vd(g_cpu, core);
    return NULL;
}

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
    }

    fclose(task_file);

    printf("Read %d tasks successfully.\n", num_tasks);

    double x_table[num_levels];
    int k_result = -1;

    if (!edf_vd_preprocess(tasks, num_tasks, x_table, &k_result))
    {
        printf("[RESULT] Task set is NOT schedulable under EDF-VD.\n");
        return -3;
    }

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

    FILE* log_file = fopen("logs/core_0.log", "w");
    if (log_file == NULL)
    {
        printf("[ERROR] Could not open logs/core_0.log for writing\n");
        return -4;
    }

    int num_cores = 1;
    CPU cpu;
    Core cores[num_cores];

    cpu.num_cores      = num_cores;
    cpu.cores          = cores;
    cpu.current_level  = 1;
    cpu.k_boundary     = k_result;
    cpu.tasks          = tasks;
    cpu.num_tasks      = num_tasks;
    cpu.ready_queue    = heap_init(64);
    cpu.log_file       = log_file;
    pthread_mutex_init(&cpu.queue_lock, NULL);
    memcpy(cpu.x_table, x_table, num_levels * sizeof(double));

    for (int i = 0; i < num_cores; i++)
    {
        cores[i].core_id   = i;
        cores[i].running_job = NULL;
        cores[i].log_file   = log_file;
    }

    srand(42);
    g_cpu = &cpu;

    pthread_t threads[num_cores];
    int core_ids[num_cores];

    for (int i = 0; i < num_cores; i++)
    {
        core_ids[i] = i;
        pthread_create(&threads[i], NULL, core_worker, &core_ids[i]);
    }

    for (int i = 0; i < num_cores; i++)
        pthread_join(threads[i], NULL);

    pthread_mutex_destroy(&cpu.queue_lock);
    heap_destroy(cpu.ready_queue);

    fclose(log_file);
    printf("[INFO] Simulation log written to logs/core_0.log\n");

    return 0;
}