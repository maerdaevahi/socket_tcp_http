#include <sys/time.h>
#include <unistd.h>
#include "concurrency.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <cerrno>
#include "thr/thread_pool.h"


typedef struct epoll_info {
    int epoll_fd;
    void * root_ecv;
    struct epoll_event * events;
    int max_count;
    int ready_count;
    int timeout;
    net_app * na;
    thread_pool * pool;
} epoll_info;

typedef struct epoll_cb_arg {
    void (*fun)(epoll_cb_arg * arg);
    epoll_info * ei;
    int fd;
} epoll_cb_arg;

epoll_cb_arg * register_event(epoll_info *ei, int peer_fd, void (*fun)(epoll_cb_arg *));
void unregister_event(epoll_cb_arg * arg);

epoll_cb_arg * register_event(epoll_info *ei, int peer_fd, void (*fun)(epoll_cb_arg *)) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    epoll_cb_arg *arg = (epoll_cb_arg *)malloc(sizeof(epoll_cb_arg));
    arg->fun = fun;
    arg->ei = ei;
    arg->fd = peer_fd;
    ev.data.ptr = arg;
    epoll_ctl(ei->epoll_fd, EPOLL_CTL_ADD, peer_fd, &ev);
    return arg;
}

void unregister_event(epoll_cb_arg * arg) {
    epoll_ctl(arg->ei->epoll_fd, EPOLL_CTL_DEL, arg->fd, NULL);
    free(arg);
}

typedef struct task_arg {
    net_app *na;
    int fd;
} task_arg;

static void handle(task_arg * arg) {
    net_app * na = arg->na;
    int fd = arg->fd;
    void *ctx = na->mk_context(fd);
    na->handle_read(ctx);
    na->handle_business(ctx);
    na->handle_write(ctx);
    na->free_context(ctx);
    free(arg);
}

void handle_read(epoll_cb_arg * arg) {
    task_arg *arg2Task = (task_arg *) malloc(sizeof(task_arg));
    arg2Task->na = arg->ei->na;
    arg2Task->fd = arg->fd;
    task task1;
    init_task(&task1, (void *(*)(void *)) handle, arg2Task);
    commit_task(arg->ei->pool, &task1);
    unregister_event(arg);
}

void handle_connection(epoll_cb_arg * arg) {
    while (1) {
        int peer_fd = arg->ei->na->handle_connection(arg->fd);
        if (peer_fd == -1) {
            perror(__func__);
            break;
        }
        register_event(arg->ei, peer_fd, handle_read);
    }
}

void init_epoll_info(epoll_info * ei, int max_count, net_app * na, int initial_threads, int max_threads, int max_tasks) {
    ei->epoll_fd = epoll_create(1);
    ei->na = na;
    ei->root_ecv = register_event(ei, ei->na->listen_fd, handle_connection);
    ei->max_count = max_count;
    ei->events = (struct epoll_event *) malloc( max_count * sizeof(struct epoll_event));
    ei->pool = (thread_pool *)malloc(sizeof(thread_pool));
    init_thread_pool(ei->pool, initial_threads, max_threads, max_tasks);
}

static void destroy_epoll_info(epoll_info * ei) {
    if (ei->root_ecv) {
        free(ei->root_ecv);
    }
    if (ei->events) {
        free(ei->events);
    }
    if (ei->pool) {
        destroy_thread_pool(ei->pool);
        free(ei->pool);
    }
}

static void run(epoll_info * ei) {
    for (;;) {
        ei->ready_count = epoll_wait(ei->epoll_fd, ei->events,ei->max_count, ei->timeout);
        if (ei->ready_count == -1 && errno != EINTR) {
            break;
        }
        for (int i = 0; i < ei->ready_count; ++i) {
           epoll_cb_arg * ecv = (epoll_cb_arg * )ei->events[i].data.ptr;
           ecv->fun(ecv);
        }
    }
}


void do_epoll(void * arg) {
    printf("%s\n", __func__);
    net_app * na = (net_app *)arg;
    epoll_info ei;
    init_epoll_info(&ei, FD_SETSIZE, na, 8, 16, 16);
    run(&ei);
    destroy_epoll_info(&ei);
}