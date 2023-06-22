#ifndef __THREAD_POOL__
#define __THREAD_POOL__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "thread_queue.h"
#include "task_collection.h"

#define CAPACITY 32
#define handle_error(en, msg) \
do {\
    if (en) {\
        printf("%s: %s\n", msg, strerror(en));\
        exit(1);\
    }\
} while (0)
typedef struct thread_pool {
    thread_queue threads;
    task_collection task_col;
    int on;
    
} thread_pool;

void init_thread_pool(thread_pool * tp, int n_threads, int max_n_threads, int max_n_tasks);
void destroy_thread_pool(thread_pool * tp);
void create_thread(thread_pool * tp, int n);
void join_thread(thread_pool * tp);
void commit_task(thread_pool * tp, task * t);
void * consume(void * arg);
#endif