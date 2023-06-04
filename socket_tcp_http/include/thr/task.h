#ifndef __TASK__
#define __TASK__
typedef struct task {
    void * (*fun)(void *);
    void * arg;
} task;
void init_task(task * t, void * (*fun)(void *), void * arg);
void assign_task(task * t, task * t1);
void execute_task(task * t);
#endif