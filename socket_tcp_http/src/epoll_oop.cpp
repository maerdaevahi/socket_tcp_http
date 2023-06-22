#include <sys/time.h>
#include <unistd.h>
#include "concurrency.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <cerrno>
#include "thr_cc/thread_pool.hpp"
#include <stdarg.h>
#include <string.h>

typedef enum {
    SYN,
    ASYN
} ev_way;

typedef enum {
    RD,
    WR
} ev_tp;

static void * ctx_set[1024] = {NULL};
class epoll_cb_arg;

class epoll_oop_info {
    friend class epoll_cb_arg;
    int epoll_fd;
    std::mutex mutex;
    epoll_cb_arg * root_eca;
    struct epoll_event * events;
    int max_count;
    int ready_count;
    int timeout;
    net_app * na;
    sword::thread_pool<sword::task> * pool;
public:
    epoll_oop_info(int max_count, net_app * na, ev_way way, ...);
    ~epoll_oop_info();
    void run();
    void process_read(int fd);
    void process_write(int fd);
};

typedef struct task_arg {
    epoll_oop_info * ei;
    int fd;
} task_arg;

class epoll_cb_arg {
    int fd;
    epoll_oop_info * ei;
public:
    void (epoll_cb_arg::*mem_fun)();
public:
    epoll_cb_arg() = default;
    epoll_cb_arg(epoll_oop_info * ei, int peer_fd, void (epoll_cb_arg::*fun)());
    void register_event(ev_tp et);
    void unregister_event();
    void handle_read_event();
    void handle_write_event();
    void handle_connection_event();
};

epoll_cb_arg::epoll_cb_arg(epoll_oop_info * ei, int peer_fd, void (epoll_cb_arg::*fun)()) {
    this->mem_fun = fun;
    this->ei = ei;
    this->fd = peer_fd;
}

void epoll_cb_arg::register_event(ev_tp et) {
    struct epoll_event ev;
    switch (et) {
        case RD:
            ev.events = EPOLLIN | EPOLLET;
            break;
        case WR:
            ev.events = EPOLLOUT | EPOLLET;
            break;
        default:
            ev.events = EPOLLIN | EPOLLET;
            break;
    }
    ev.data.ptr = this;
    std::mutex * mtx = &this->ei->mutex;
    mtx->lock();
    epoll_ctl(this->ei->epoll_fd, EPOLL_CTL_ADD, this->fd, &ev);
    mtx->unlock();
}

void epoll_cb_arg::unregister_event() {
    std::mutex * mtx = &this->ei->mutex;
    mtx->lock();
    epoll_ctl(this->ei->epoll_fd, EPOLL_CTL_DEL, this->fd, NULL);
    mtx->unlock();
    //free(this);
    delete this;
}

void epoll_oop_info::process_read(int fd) {
    void *ctx = ctx_set[fd];
    if (!ctx) {
        fprintf(stderr, "resource is not allocated\n");
        exit(1);
    }
    this->na->handle_read(ctx);
    this->na->handle_business(ctx);
    /*na->handle_write(ctx);
    na->free_context(ctx);*/
    printf("_______________________________________%s______________________________________\n", __func__);
    epoll_cb_arg * read_arg = new epoll_cb_arg(this, fd, &epoll_cb_arg::handle_write_event);
    read_arg->register_event(WR);
    printf("=======================================%s=======================================\n", __func__);
}

void epoll_oop_info::process_write(int fd) {
    printf("***************************************%s**************************************\n", __func__);
    void *ctx = ctx_set[fd];
    if (!ctx) {
        fprintf(stderr, "resource is not allocated\n");
        exit(1);
    }
    ctx_set[fd] = NULL;
    this->na->handle_write(ctx);
    this->na->free_context(ctx);
}

static void process_read_asyn(task_arg * arg) {
    epoll_oop_info * na = arg->ei;
    int fd = arg->fd;
    na->process_read(fd);
    free(arg);
}

void epoll_cb_arg::handle_read_event() {
    printf("%s: %p, epoll_cb_arg * arg %p\n", __func__, &epoll_cb_arg::handle_read_event, this->mem_fun);
    if (this->ei->pool) {
        task_arg *arg2Task = (task_arg *) malloc(sizeof(task_arg));
        arg2Task->ei = this->ei;
        arg2Task->fd = this->fd;
        sword::task task1((void *(*)(void *)) process_read_asyn, arg2Task);
        this->ei->pool->commit_task(task1);
    } else {
        this->ei->process_read(this->fd);
    }
    this->unregister_event();
}

static void process_write_asyn(task_arg * arg) {
    epoll_oop_info * na = arg->ei;
    int fd = arg->fd;
    na->process_write(fd);
    free(arg);
}

void epoll_cb_arg::handle_write_event() {
    printf("%s: %p, epoll_cb_arg * arg %p\n", __func__, &epoll_cb_arg::handle_write_event, this->mem_fun);
    if (this->ei->pool) {
        task_arg *arg2Task = (task_arg *) malloc(sizeof(task_arg));
        arg2Task->ei = this->ei;
        arg2Task->fd = this->fd;
        sword::task task1((void *(*)(void *)) process_write_asyn, arg2Task);
        this->ei->pool->commit_task(task1);
    } else {
        this->ei->process_write(this->fd);
    }
    this->unregister_event();
}

void epoll_cb_arg::handle_connection_event() {
    printf("%s: %p, epoll_cb_arg * arg %p\n", __func__, &epoll_cb_arg::handle_connection_event, this->mem_fun);
#if USE_CONCURRENCY
    while (1) {
#endif
        int peer_fd = this->ei->na->handle_connection(this->fd);
        if (peer_fd == -1) {
            perror(__func__);
#if USE_CONCURRENCY
            break;
#endif
        }
        epoll_cb_arg * read_arg = new epoll_cb_arg(this->ei, peer_fd, &epoll_cb_arg::handle_read_event);
        read_arg->register_event(RD);
        void *ctx = this->ei->na->mk_context(peer_fd);
        if (ctx_set[peer_fd]) {
            fprintf(stderr, "resource is not clean\n");
            exit(1);
        } else {
            ctx_set[peer_fd] = ctx;
        }
#if USE_CONCURRENCY
    }
#endif
}

epoll_oop_info::epoll_oop_info(int max_count, net_app * na, ev_way way, ...) {
    this->epoll_fd = epoll_create(1);
    this->timeout = -1;
    this->na = na;
    this->root_eca = new epoll_cb_arg(this, na->listen_fd, &epoll_cb_arg::handle_connection_event);
    this->root_eca->register_event(RD);
    this->max_count = max_count;
    this->events = (struct epoll_event *) malloc( max_count * sizeof(struct epoll_event));
    if (way == ASYN) {
        va_list valist;
        va_start(valist, way);
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
        this->pool = new sword::thread_pool<>(initial_threads, max_tasks);
    } else if (way == SYN) {
        this->pool = NULL;
    }
}

epoll_oop_info::~epoll_oop_info() {
    if (root_eca) {
        free(root_eca);
    }
    if (events) {
        free(events);
    }
    delete pool;
}

void epoll_oop_info::run() {
    for (;;) {
        ready_count = epoll_wait(epoll_fd, events,max_count, timeout);
        if (ready_count == -1 && errno != EINTR) {
            break;
        }
        for (int i = 0; i < ready_count; ++i) {
            epoll_cb_arg * ecv = (epoll_cb_arg * )events[i].data.ptr;
            void (epoll_cb_arg::*mem_fun)() = ecv->mem_fun;
            (ecv->*mem_fun)();
        }
    }
}


void do_epoll_oop(void * arg) {
    printf("%s\n", __func__);
    net_app * na = (net_app *)arg;
#if USE_CONCURRENCY
    epoll_oop_info ei(FD_SETSIZE, na, ASYN, 8, 16, 16);
#else
    epoll_oop_info ei(FD_SETSIZE, na, SYN);
#endif
    ei.run();
}