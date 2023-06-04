#include "thr/task.h"
void assign_task(task * t, task * t1) {
    init_task(t, t1->fun, t1->arg);
}

void execute_task(task * t) {
    t->fun(t->arg);
}

void init_task(task * t, void * (*fun)(void *), void * arg) {
    t->fun = fun;
    t->arg = arg;
}