#ifndef __TASK_QUEUE__
#define __TASK_QUEUE__
#include "task.h"
typedef struct task_queue {
    task * container;
    int size;
    int start, end;
} task_queue;
void init_task_queue(task_queue * cq, int size);
void destroy_task_queue(task_queue * cq);
int test_task_queue_full(task_queue * cq);
int test_task_queue_empty(task_queue * cq);
void add_into_task_queue(task_queue * cq, task * e);
void remove_from_task_queue(task_queue * cq, task * t);
int count_task(task_queue * cq);
#endif