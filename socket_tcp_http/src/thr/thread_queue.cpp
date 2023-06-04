#include <stdlib.h>
#include "thr/thread_queue.h"
void init_thread_queue(thread_queue * cq, int size) {
    cq->start = 0;
    cq->end = 0;
    cq->size = size;
    cq->container = (thread *)malloc(size * sizeof(thread));
}

void destroy_thread_queue(thread_queue * cq) {
    thread * ptr = cq->container;
    if (ptr) {
        free(ptr);
        ptr = NULL;
    }
}

int test_thread_queue_full(thread_queue * cq) {
    return (cq->end + 1) % cq->size == cq->start;
}

int test_thread_queue_empty(thread_queue * cq) {
    return cq->start == cq->end;
}

void add_into_thread_queue(thread_queue * cq, thread * e) {
    thread * ptr = &cq->container[cq->end++];
    assign_thread(ptr, e);
    cq->end %= cq->size;
}

void remove_from_thread_queue(thread_queue * cq, thread * t) {
    thread * e = &cq->container[cq->start++];
    assign_thread(t, e);
    cq->start %= cq->size;
}

int count_thread(thread_queue *  cq) {
    return (cq->end -cq->start + cq->size) % cq->size;
}