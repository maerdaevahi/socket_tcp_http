#include <pthread.h>
#include <errno.h>
#include "thr/thread_pool.h"

void init_thread_pool(thread_pool * tp, int n_threads, int max_n_threads, int max_n_tasks) {
    if (n_threads <= 0 || max_n_threads <=0 || max_n_tasks <= 0 || n_threads > max_n_threads) {
        handle_error(EINVAL, __func__);
    }
    init_thread_queue(&tp->threads, max_n_threads);
    init_task_collection(&tp->task_col, max_n_tasks);
    create_thread(tp, n_threads);
}

void destroy_thread_pool(thread_pool * tp) {
    destroy_thread_queue(&tp->threads);
    destroy_task_collection(&tp->task_col);
}

void create_thread(thread_pool * tp, int n) {
    pthread_t thread_id;
    pthread_attr_t attr;
    int ret = pthread_attr_init(&attr);
    handle_error(ret, "pthread_attr_init");
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    while (n--) {
        ret = pthread_create(&thread_id, &attr, consume, tp);
        handle_error(ret, "pthread_create");    
        thread thr;
        thr.thread_id = thread_id;
        add_into_thread_queue(&tp->threads, &thr); 
    }
}

void join_thread(thread_pool * tp) {
    while (1) {
        if (test_thread_queue_empty(&tp->threads)) {
            break;
        }
        thread thr;
        remove_from_thread_queue(&tp->threads, &thr);
        pthread_join(thr.thread_id, NULL);
    }
}

void commit_task(thread_pool * tp, task * t) {
    submit_task(&tp->task_col, t);
}

void * consume(void * arg) {
    task_collection * resrc = &((thread_pool *)arg)->task_col;
    while (1) {
        task t;
        acquire_task(resrc, &t);
        execute_task(&t);
    }
}