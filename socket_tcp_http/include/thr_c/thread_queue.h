#ifndef __THREAD_QUEUE__
#define __THREAD_QUEUE__
#include "thread.h"
typedef struct thread_queue {
    thread * container;
    int size;
    int start, end;
} thread_queue;
void init_thread_queue(thread_queue * cq, int size);
void destroy_thread_queue(thread_queue * cq);
int test_thread_queue_full(thread_queue * cq);
int test_thread_queue_empty(thread_queue * cq);
void add_into_thread_queue(thread_queue * cq, thread * e);
void remove_from_thread_queue(thread_queue * cq, thread * t);
int count_thread(thread_queue *  cq);
#endif