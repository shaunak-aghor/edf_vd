#include "../include/common.h"
#include "../include/min_heap.h"
//Helper Functions

void swap(HeapNode* a, HeapNode* b) {
    HeapNode temp = *a;
    *a = *b;
    *b = temp;
}

// Restore Heap Property Upwards (after push)
void bubble_up(MinHeap* heap, int index) {
    while (index > 0) {
        int parent_idx = (index - 1) / 2;
        
        // If current is smaller than parent, swap
        if (heap->data[index].priority < heap->data[parent_idx].priority) {
            swap(&heap->data[index], &heap->data[parent_idx]);
            index = parent_idx;
        } else {
            break; 
        }
    }
}

// Restore Heap Property Downwards (after pop)
void bubble_down(MinHeap* heap, int index) {
    int smallest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;

    if (left < heap->size && heap->data[left].priority < heap->data[smallest].priority)
        smallest = left;

    if (right < heap->size && heap->data[right].priority < heap->data[smallest].priority)
        smallest = right;

    if (smallest != index) {
        swap(&heap->data[index], &heap->data[smallest]);
        bubble_down(heap, smallest);
    }
}

// --- Implementation ---

MinHeap* heap_init(int capacity) {
    MinHeap* heap = (MinHeap*) malloc(sizeof(MinHeap));
    if (!heap) return NULL;

    heap->data = (HeapNode*) malloc(sizeof(HeapNode) * capacity);
    heap->size = 0;
    heap->capacity = capacity;
    return heap;
}

void heap_destroy(MinHeap* heap) {
    if (heap) {
        free(heap->data);
        free(heap);
    }
}

void heap_push(MinHeap* heap, Job* job, double priority) {
    // Auto-resize if full
    if (heap->size == heap->capacity) {
        heap->capacity *= 2;
        heap->data = (HeapNode*) realloc(heap->data, sizeof(HeapNode) * heap->capacity);
    }

    // Insert at end
    int index = heap->size;
    heap->data[index].job = job;
    heap->data[index].priority = priority;
    heap->size++;

    bubble_up(heap, index);
}

Job* heap_pop(MinHeap* heap) {
    if (heap->size == 0) return NULL;

    Job* top_job = heap->data[0].job;

    // Move last element to root
    heap->data[0] = heap->data[heap->size - 1];
    heap->size--;

    if (heap->size > 0) {
        bubble_down(heap, 0);
    }

    return top_job;
}

Job* heap_peek(MinHeap* heap) {
    if (heap->size == 0) return NULL;
    return heap->data[0].job;
}

bool heap_is_empty(MinHeap* heap) {
    return (heap->size == 0);
}

void update_heap_for_mode_switch(MinHeap* heap, int current_level, int k_boundary) {
    int i = 0;
    while (i < heap->size) {
        if (heap->data[i].job->task->level < current_level) {
            // Task is too low criticality. Drop the job!
            free(heap->data[i].job);
            
            // Swap this slot with the last element in the heap array
            heap->data[i] = heap->data[heap->size - 1];
            heap->size--;
            
            
            
        } else {
            
            if (current_level > k_boundary) {
                heap->data[i].job->absolute_deadline = (double)heap->data[i].job->arrival_time + heap->data[i].job->task->virtual_deadline;
                heap->data[i].priority = heap->data[i].job->absolute_deadline; 
            }
            i++;
        }
    }

    // Re-Heapify (Build-Heap algorithm)
    for (int j = (heap->size / 2) - 1; j >= 0; j--) 
    {
        bubble_down(heap, j);
    }
}