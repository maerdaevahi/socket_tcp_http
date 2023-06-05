#include <sys/time.h>
#include <unistd.h>
#include "concurrency.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <cerrno>
#include "thr/thread_pool.h"
#include <stdarg.h>

typedef enum {
    SYN,
    ASYN
} ev_way;


typedef struct epoll_info {
    int epoll_fd;
    void * root_eca;
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
void handle_read(epoll_cb_arg * arg);

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

static void handle(net_app * na, int fd) {
    void *ctx = na->mk_context(fd);
    na->handle_read(ctx);
    na->handle_business(ctx);
    na->handle_write(ctx);
    na->free_context(ctx);
}

static void handle_asyn(task_arg * arg) {
    net_app * na = arg->na;
    int fd = arg->fd;
    handle(na, fd);
    free(arg);
}

void handle_connection(epoll_cb_arg * arg) {
    printf("%s: %p, epoll_cb_arg * arg %p\n", __func__, handle_connection, arg->fun);
#if USE_CONCURRENCY
    while (1) {
#endif
        int peer_fd = arg->ei->na->handle_connection(arg->fd);
        if (peer_fd == -1) {
            perror(__func__);
#if USE_CONCURRENCY
            break;
#endif
        }
        register_event(arg->ei, peer_fd, handle_read);
#if USE_CONCURRENCY
    }
#endif
}

void handle_read(epoll_cb_arg * arg) {
    printf("%s: %p, epoll_cb_arg * arg %p\n", __func__, handle_read, arg->fun);
    if (arg->ei->pool) {
        task_arg *arg2Task = (task_arg *) malloc(sizeof(task_arg));
        arg2Task->na = arg->ei->na;
        arg2Task->fd = arg->fd;
        task task1;
        init_task(&task1, (void *(*)(void *)) handle_asyn, arg2Task);
        commit_task(arg->ei->pool, &task1);
    } else {
        handle(arg->ei->na, arg->fd);
    }
    unregister_event(arg);
}

void init_epoll_info(epoll_info * ei, int max_count, net_app * na, ev_way way, ...) {
//void init_epoll_info(epoll_info * ei, int max_count, net_app * na, ev_way way, int initial_threads, int max_threads, int max_tasks) {
    ei->epoll_fd = epoll_create(1);
    ei->na = na;
    ei->root_eca = register_event(ei, ei->na->listen_fd, handle_connection);
    ei->max_count = max_count;
    ei->events = (struct epoll_event *) malloc( max_count * sizeof(struct epoll_event));
    if (way == ASYN) {
        va_list valist;
        va_start(valist, way);
        ei->pool = (thread_pool *)malloc(sizeof(thread_pool));
        if (valist) {

        }
        int initial_threads = 1, max_threads = 1, max_tasks = 1;
        do {
            int n = va_arg(valist, int);
            if (n <= 0) {
                break;
            }
            initial_threads = n;
            n = va_arg(valist, int);
            if (n <= 0) {
                break;
            }
            max_threads = n;
            n = va_arg(valist, int);
            if (n <= 0) {
                break;
            }
            max_tasks = n;
        } while (0);
        init_thread_pool(ei->pool, initial_threads, max_threads, max_tasks);
    } else if (way == SYN) {
        ei->pool = NULL;
    }
}

static void destroy_epoll_info(epoll_info * ei) {
    if (ei->root_eca) {
        free(ei->root_eca);
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
#if USE_CONCURRENCY
    init_epoll_info(&ei, FD_SETSIZE, na, ASYN, 8, 16, 16);
#else
    init_epoll_info(&ei, FD_SETSIZE, na, SYN);
#endif
    run(&ei);
    destroy_epoll_info(&ei);
}