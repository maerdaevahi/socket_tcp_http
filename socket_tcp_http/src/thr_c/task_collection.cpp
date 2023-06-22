#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "thr_c/task_collection.h"
#define handle_error(en, msg) \
do {\
    if (en) {\
        printf("%s: %s\n", msg, strerror(en));\
        exit(1);\
    }\
} while (0)
void init_task_collection(task_collection * resrc, int max_n_tasks) {
    init_task_queue(&resrc->cq, max_n_tasks);
    pthread_mutexattr_t mutexattr;
    int ret = pthread_mutexattr_init(&mutexattr);
    handle_error(ret, "pthread_mutexattr_init");
    ret = pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_PRIVATE);
    handle_error(ret, "pthread_mutexattr_setpshared");
    ret = pthread_mutex_init(&resrc->mutex, &mutexattr);
    handle_error(ret, "pthread_mutex_init");
    ret = pthread_mutexattr_destroy(&mutexattr);
    handle_error(ret, "pthread_mutexattr_destroy");

    pthread_condattr_t condattr;
    ret = pthread_condattr_init(&condattr);
    handle_error(ret, "pthread_condattr_init");
    ret = pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_PRIVATE);
    handle_error(ret, "pthread_condattr_setpshared");
    pthread_cond_t not_empty, not_full;
    ret = pthread_cond_init(&resrc->not_empty, &condattr);
    handle_error(ret, "pthread_cond_init");
    ret = pthread_cond_init(&resrc->not_full, &condattr);
    handle_error(ret, "pthread_cond_init");
    ret = pthread_condattr_destroy(&condattr);
    handle_error(ret, "pthread_condattr_destroy");    
}

void destroy_task_collection(task_collection * resrc) {
    destroy_task_queue(&resrc->cq);
    pthread_mutex_destroy(&resrc->mutex);
    pthread_cond_destroy(&resrc->not_empty);
    pthread_cond_destroy(&resrc->not_full);
}

void submit_task(task_collection * resrc, task * t) {
    pthread_t thr_id = pthread_self();
    //printf("thread [%ld] %s %p\n", thr_id, __func__, resrc);
    pthread_mutex_lock(&resrc->mutex);
    while (test_task_queue_full(&resrc->cq)) {
        pthread_cond_wait(&resrc->not_full, &resrc->mutex);
    }
    
    add_into_task_queue(&resrc->cq, t);
    printf("%ld write %d %ld\n", thr_id, resrc->cq.end, (long)t->arg);
    pthread_cond_signal(&resrc->not_empty);
    pthread_mutex_unlock(&resrc->mutex);
}

void acquire_task(task_collection * resrc, task * t) {
    pthread_t thr_id = pthread_self();
    //printf("thread [%ld] %s %p\n", thr_id, __func__, resrc);
    pthread_mutex_lock(&resrc->mutex);
    while (test_task_queue_empty(&resrc->cq)) {
        pthread_cond_wait(&resrc->not_empty, &resrc->mutex);
    }
    remove_from_task_queue(&resrc->cq, t);
    //execute_task(&t);
    
    //printf("%ld read %d\n", thr_id, resrc->cq.start);
    pthread_cond_signal(&resrc->not_full);
    pthread_mutex_unlock(&resrc->mutex);
}