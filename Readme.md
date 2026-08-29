# EDF-VD single-core simulator

This project is a compact EDF-VD scheduler simulator for a single core. It follows the usual EDF-VD pattern from the paper:

- an offline feasibility phase computes the criticality boundary and scaling factor `x`
- an online runtime phase schedules jobs using EDF ordering
- when a job exceeds its current criticality budget, the system raises its level and decides which tasks are dropped or have their virtual deadlines stretched

At the moment, the implementation is intentionally single-core only. There is one ready queue, one running job, and one scheduler state. The multicore extension is not implemented yet.

---

## 1. High-level idea

EDF-VD stands for Earliest Deadline First with Virtual Deadlines.

The idea is:

- tasks may have different WCETs at different criticality levels
- low-criticality tasks are allowed to run with their nominal deadlines while the system is in a lower mode
- when the system becomes more critical, the deadlines of higher-criticality tasks are scaled by a factor `x`
- tasks below the mode threshold are dropped

In this codebase, the criticality system is simplified to `num_levels = 2`, so the runtime is basically:

- Level 1: nominal execution / low mode
- Level 2: higher criticality / virtual-deadline mode

---

## 2. Relevant files and responsibilities

- `src/main.c`  
  Reads the task input, builds the task tables, runs preprocessing, and starts the runtime simulation.

- `src/edf_vd.c`  
  Contains the offline preprocessing logic and the main runtime loop (`simulate_edf_vd`).

- `src/utils.c`  
  Contains the timing helpers and the event handlers:
  - job arrival
  - job completion
  - mode switch
  - deadline/preemption updates

- `include/task_job.h`  
  Data model for tasks, jobs, and the core state.

- `include/min_heap.h` and `src/min_heap.c`  
  Min-heap used as the EDF ready queue.

---

## 3. Offline phase: how the algorithm decides the scaling

The offline phase is implemented by `edf_vd_preprocess` in `src/edf_vd.c`.

### What it does

It computes:

- whether the task set is already schedulable at level 1
- if not, the mode boundary `k`
- the scaling factor `x` for the higher-criticality tasks
- initial virtual deadlines for each task

### Important detail

The code uses a simplified two-level model:

- for a task with `level <= k`, its deadline stays based on its normal period
- for a task with `level > k`, its deadline becomes `x * period`

The actual preprocessing logic is:

1. Sum utilization for tasks at their highest-level WCET.
2. If total utilization is already `<= 1`, it returns immediately and says no scaling is needed.
3. Otherwise, it tries different boundaries `k` and computes values such as:
   - `u_lo_lo`: utilization of low-level tasks at their own WCET
   - `u_hi_k`: utilization of higher-level tasks at the lower-level WCET
   - `u_hi_hi`: utilization of higher-level tasks at their own WCET
4. It derives an admissible `x_min` and checks whether `x_min <= x_max`.
5. If valid, it stores the `x` value and assigns each task a virtual deadline.

### Why this is EDF-VD

The paper’s idea is to avoid making the system over-constrained in the high-criticality state. Instead of using the original deadlines for all tasks, the algorithm chooses a scaling factor so that the higher-criticality jobs still have enough slack while the lower-criticality jobs are dropped or deprioritized.

### Example of the offline phase

Take an illustrative task set:

- T1: level 1, period 100, wcet = 20
- T2: level 2, period 100, wcet at level 1 = 30, wcet at level 2 = 60
- T3: level 2, period 100, wcet at level 1 = 40, wcet at level 2 = 80

At the highest level:

- U_hi_hi = 20/100 + 60/100 + 80/100 = 1.6

This is above 1, so the scheduler must find a safe `x`.

Suppose the boundary is `k = 1`:

- low tasks are just T1
- high tasks are T2 and T3

Then the code computes the aggregate load at the low mode and checks:

- `u_lo_lo = 20/100 = 0.2`
- `u_hi_k = 30/100 + 40/100 = 0.7`

So roughly:

- `x_min = u_hi_k / (1 - u_lo_lo) = 0.7 / 0.8 = 0.875`

If this is feasible, it sets the high-criticality tasks’ virtual deadlines to:

- `virtual_deadline = x * period = 0.875 * 100 = 87.5`

This means those tasks are effectively treated as if they had a shorter deadline, but only after the system enters the elevated criticality state.

### Current repo behavior

In the sample inputs, especially `inputs/taskset1.txt`, the utilization often ends up not requiring scaling, so the code falls into the simpler path:

- `k_boundary = 0`
- `x_table = [1.0, 1.0]`
- `virtual_deadline = period`

This makes the runtime behave almost like ordinary EDF, which is a valid single-core baseline for the simulator.

---

## 4. Online phase: runtime execution

The online phase is driven by `simulate_edf_vd` in `src/edf_vd.c`.

### The main runtime loop

At each step, the simulator does this:

1. finds the next task arrival
2. finds the next completion of the running job
3. finds the next mode-switch event
4. takes the minimum of those times
5. advances time to that event
6. updates running execution
7. triggers the right event handler
8. dispatches or preempts the CPU according to EDF

### Runtime state

The core state is defined in `include/task_job.h`:

- `current_level`  → current criticality level
- `k_boundary`    → the boundary selected in preprocessing
- `running_job`   → currently running job on the single core
- `ready_queue`   → EDF min-heap ordered by absolute deadline
- `x_table`       → scaling values for each level

### The runtime is event-driven

The simulator does not run in a continuous time loop per se. It jumps from one significant event to another:

- job arrival
- job completion
- mode switch
- preemption

This makes the implementation easier to reason about and matches the way EDF-VD is usually described in paper-style pseudocode.

---

## 5. Event handlers and what each one does

The event handlers are in `src/utils.c`.

### a) `handle_job_arrival`

This is triggered when a task reaches its `next_arrival_time`.

What it does:

- scans all active tasks
- when a task’s arrival time matches `current_time`, it creates a `Job`
- assigns:
  - job id
  - task pointer
  - arrival time
  - `time_executed = 0`
  - random execution demand up to the task’s current WCET
  - `absolute_deadline = current_time + task.virtual_deadline`
- pushes the new job into the ready heap

This is the “spawn new job at release time” part of EDF scheduling.

### b) `handle_job_completion`

This is called when the running job has consumed its remaining execution time.

What it does:

- checks whether `running_job != NULL`
- logs completion
- frees the `Job`
- sets `running_job = NULL`

This is the normal termination event for a job.

### c) `get_next_mode_switch_time`

This is not exactly a handler, but it decides whether a level switch is needed.

The logic is:

- compute the remaining budget for the current mode
- if the job would exceed that budget before finishing, schedule a mode switch at `current_time + budget_remaining`

This is the criterion used to trigger a criticality escalation.

### d) `handle_mode_switch`

This is the most important online event.

What it does:

1. increments `current_level`
2. logs the level increase
3. reads the correct `x` value from `x_table`
4. updates every active task:
   - if a task is below the new level, it is dropped as permanently inactive
   - if it remains active at the new level, its `virtual_deadline` is recomputed using the new `x`
5. handles the currently running job:
   - if it belongs to a dropped task, it is killed immediately
   - if it survives, its deadline may be updated to the new virtual deadline
6. reorders the heap using `update_heap_for_mode_switch`

This is the exact online criticality transition from the EDF-VD idea.

### e) EDF dispatch and preemption

After handling events, the dispatcher checks:

- if the CPU is idle and the heap is non-empty, run the earliest-deadline job
- if a queued job has an earlier absolute deadline than the current job, preempt the running one and dispatch the new one

This is classic EDF behavior.

---

## 6. Example of the full runtime flow

Here is a good step-by-step example of the current single-core logic.

Assume two tasks:

- T1: level 1, period 100, wcet at level 1 = 20
- T2: level 2, period 100, wcet at level 1 = 30, wcet at level 2 = 60

And the preprocessing selected `k = 1` and `x = 0.875`.

### At time `t = 0`

- both tasks release jobs
- each job gets an absolute deadline:
  - `T1`: `0 + 100 = 100`
  - `T2`: `0 + 0.875 * 100 = 87.5`
- the heap orders them by deadline, so T2 runs first

### At time `t = 10`

- T2 has consumed some execution but not yet reached its level-1 budget
- suppose T2 crosses its level-1 WCET threshold at `t = 30`

### At time `t = 30`

- `get_next_mode_switch_time` triggers a mode switch
- system level rises from 1 to 2
- `handle_mode_switch` does:
  - updates `current_level = 2`
  - uses `x = 0.875` for virtual deadlines
  - drops any task with `level < 2`
  - recomputes `virtual_deadline` for surviving tasks
  - updates the running job if needed

If `T1` was low-criticality, it is dropped.
If `T2` remains active, its absolute deadline is reset based on the new virtual deadline.

### After mode switch

- the ready queue is reordered
- the CPU may preempt the current job if another active job has earlier deadline
- the simulation continues until the hyperperiod or until no jobs remain

This is exactly the dynamic online behavior implemented by the code.

---

## 7. What is not implemented yet

The code currently does not include:

- multicore scheduling
- per-core partitioning
- global job migration
- multiple independent scheduler instances
- a separate `core_1`, `core_2`, etc. execution model

So the project right now is a faithful single-core prototype of the EDF-VD behavior, not the multicore paper extension.

---

## 8. Very short pseudo-flow

```text
read task input

OFFLINE:
  compute total utilization
  if feasible: x = 1.0, virtual_deadline = period
  else:
    find k and x
    assign virtual deadlines

ONLINE:
  while time < hyperperiod:
    find next arrival / completion / mode-switch
    advance to next event
    update running job execution

    if completion:
      handle_job_completion()

    if mode switch:
      handle_mode_switch()

    if arrival:
      handle_job_arrival()

    dispatch or preempt using EDF heap
```

---

## 9. Summary

The current implementation is best understood as:

- an EDF scheduler with a heap-based ready queue
- a criticality-aware mode switch triggered when budgets are exceeded
- a preprocessing phase that chooses a safe `x` and `k_boundary`
- a runtime phase that reacts to arrivals, completions, and mode transitions

The essential idea is simple: the scheduler keeps the CPU busy with the earliest-deadline job, but if a job violates the current criticality budget, it raises the mode, drops low-criticality tasks, and re-evaluates deadlines for the surviving higher-criticality tasks.

That is the current EDF-VD flow in this repo.
