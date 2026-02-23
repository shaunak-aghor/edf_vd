#ifndef MIN_HEAP_H
#define MIN_HEAP_H

#include "common.h"



//lower the value, higher the priority
typedef struct {
    Job* job;
    double priority;
} HeapNode;


typedef struct {
    HeapNode* data;
    int size;      
    int capacity;  
} MinHeap;



// Initialize the heap with a specific capacity
MinHeap* heap_init(int capacity);

// Free memory (does not free the Jobs themselves)
void heap_destroy(MinHeap* heap);

// Insert a job with a specific priority
void heap_push(MinHeap* heap, Job* job, double priority);

// Remove and return the job with the lowest priority value
Job* heap_pop(MinHeap* heap);

// Return the job with the lowest priority value without removing it
Job* heap_peek(MinHeap* heap);

// Check if heap is empty
bool heap_is_empty(MinHeap* heap);


void update_heap_for_mode_switch(MinHeap* heap, int current_level, int k_boundary);

#endif