#ifndef __TASK_COLLECTION__
#define __TASK_COLLECTION__
#include <pthread.h>
#include "task_queue.h"
typedef struct task_collection {
    task_queue cq;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty, not_full;
} task_collection;
void init_task_collection(task_collection * resrc, int max_n_tasks);
void destroy_task_collection(task_collection * resrc);
void submit_task(task_collection * resrc, task * t);
void acquire_task(task_collection * resrc, task * t);
#endif