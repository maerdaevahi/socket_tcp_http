#include <stdlib.h>
#include "thr_c/task_queue.h"
void init_task_queue(task_queue * cq, int size) {
    cq->start = 0;
    cq->end = 0;
    cq->size = size;
    cq->container = (task *)malloc(size * sizeof(task));
}

void destroy_task_queue(task_queue * cq) {
    task * ptr = cq->container;
    if (ptr) {
        free(ptr);
        ptr = NULL;
    }
}

int test_task_queue_full(task_queue * cq) {
    return (cq->end + 1) % cq->size == cq->start;
}

int test_task_queue_empty(task_queue * cq) {
    return cq->start == cq->end;
}

void add_into_task_queue(task_queue * cq, task * e) {
    task * ptr = &cq->container[cq->end++];
    assign_task(ptr, e);
    cq->end %= cq->size;
}

void remove_from_task_queue(task_queue * cq, task * t) {
    task * e = &cq->container[cq->start++];
    assign_task(t, e);
    cq->start %= cq->size;
}

int count_task(task_queue * cq) {
    return (cq->end -cq->start + cq->size) % cq->size;
}