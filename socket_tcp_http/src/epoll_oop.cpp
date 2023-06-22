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

#define handle_error(en, msg) \
do {\
    if (en) {\
        printf("%s: %s\n", msg, strerror(en));\
        exit(1);\
    }\
} while (0)

typedef enum {
    SYN,
    ASYN
} ev_way;

typedef enum {
    RD,
    WR
} ev_tp;

static void * ctx_set[1024] = {NULL};

struct epoll_oop_info {
    int epoll_fd;
    std::mutex mutex;
    void * root_eca;
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

struct epoll_cb_arg {
    void (*fun)(epoll_cb_arg * arg);
    void (epoll_cb_arg::*mem_fun)();
    epoll_oop_info * ei;
    int fd;
    epoll_cb_arg() = default;
    epoll_cb_arg(epoll_oop_info * ei, int peer_fd, void (*fun)(epoll_cb_arg *));
    void register_event(ev_tp et);
    void unregister_event();
    void handle_write_event();
};

static epoll_cb_arg * build_epoll_cb_arg(epoll_oop_info * ei, int peer_fd, void (*fun)(epoll_cb_arg *));
static void register_event(epoll_cb_arg * arg, ev_tp et);
static void handle_write_event(epoll_cb_arg * arg);
static void unregister_event(epoll_cb_arg * arg);
static void handle_read_event(epoll_cb_arg * arg);
static void process_write(epoll_oop_info * ei, int fd);
static void process_write_asyn(task_arg * arg);

epoll_cb_arg::epoll_cb_arg(epoll_oop_info * ei, int peer_fd, void (*fun)(epoll_cb_arg *)) {
    this->fun = fun;
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

void epoll_cb_arg::handle_write_event() {
    printf("%s: %p, epoll_cb_arg * arg %p\n", __func__, &epoll_cb_arg::handle_write_event, this->mem_fun);
    if (this->ei->pool) {
        task_arg *arg2Task = (task_arg *) malloc(sizeof(task_arg));
        arg2Task->ei = this->ei;
        arg2Task->fd = this->fd;
        sword::task task1((void *(*)(void *)) ::process_write_asyn, arg2Task);
        this->ei->pool->commit_task(task1);
    } else {
#if 0
        process_write(this->ei, this->fd);
#else
        this->ei->process_write(this->fd);
#endif
    }
    this->unregister_event();
}

static void register_event(epoll_cb_arg * arg, ev_tp et) {
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
    ev.data.ptr = arg;
    std::mutex * mtx = &arg->ei->mutex;
    mtx->lock();
    epoll_ctl(arg->ei->epoll_fd, EPOLL_CTL_ADD, arg->fd, &ev);
    mtx->unlock();
}

static epoll_cb_arg * build_epoll_cb_arg(epoll_oop_info * ei, int peer_fd, void (*fun)(epoll_cb_arg *)) {
    epoll_cb_arg *arg = (epoll_cb_arg *)malloc(sizeof(epoll_cb_arg));
    arg->fun = fun;
    arg->ei = ei;
    arg->fd = peer_fd;
    return arg;
}

static void unregister_event(epoll_cb_arg * arg) {
    if (!arg) {
        return;
    }
    std::mutex * mtx = &arg->ei->mutex;
    mtx->lock();
    epoll_ctl(arg->ei->epoll_fd, EPOLL_CTL_DEL, arg->fd, NULL);
    mtx->unlock();
    free(arg);
}

static void process_read(epoll_oop_info * ei, int fd) {
    void *ctx = ctx_set[fd];
    if (!ctx) {
        fprintf(stderr, "resource is not allocated\n");
        exit(1);
    }
    ei->na->handle_read(ctx);
    ei->na->handle_business(ctx);
    /*na->handle_write(ctx);
    na->free_context(ctx);*/
    printf("_______________________________________%s______________________________________\n", __func__);
    epoll_cb_arg * read_arg = build_epoll_cb_arg(ei, fd, handle_write_event);
    register_event(read_arg, WR);
    printf("=======================================%s=======================================\n", __func__);
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
    epoll_cb_arg * read_arg = new epoll_cb_arg(this, fd, handle_write_event);
    register_event(read_arg, WR);
    printf("=======================================%s=======================================\n", __func__);
}

static void process_write(epoll_oop_info * ei, int fd) {
    printf("***************************************%s**************************************\n", __func__);
    void *ctx = ctx_set[fd];
    if (!ctx) {
        fprintf(stderr, "resource is not allocated\n");
        exit(1);
    }
    ctx_set[fd] = NULL;
    ei->na->handle_write(ctx);
    ei->na->free_context(ctx);
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
#if 0
    process_read(na, fd);
#else
    na->process_read(fd);
#endif
    free(arg);
}

static void process_write_asyn(task_arg * arg) {
    epoll_oop_info * na = arg->ei;
    int fd = arg->fd;
#if 0
    process_write(na, fd);
#else
    na->process_write(fd);
#endif
    free(arg);
}

static void handle_connection_event(epoll_cb_arg * arg) {
    printf("%s: %p, epoll_cb_arg * arg %p\n", __func__, handle_connection_event, arg->fun);
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
        epoll_cb_arg * read_arg = build_epoll_cb_arg(arg->ei, peer_fd, handle_read_event);
        register_event(read_arg, RD);
        void *ctx = arg->ei->na->mk_context(peer_fd);
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

static void handle_read_event(epoll_cb_arg * arg) {
    printf("%s: %p, epoll_cb_arg * arg %p\n", __func__, handle_read_event, arg->fun);
    if (arg->ei->pool) {
        task_arg *arg2Task = (task_arg *) malloc(sizeof(task_arg));
        arg2Task->ei = arg->ei;
        arg2Task->fd = arg->fd;
        sword::task task1((void *(*)(void *)) process_read_asyn, arg2Task);
        arg->ei->pool->commit_task(task1);
    } else {
#if 0
        process_read(arg->ei, arg->fd);
#else
        arg->ei->process_read(arg->fd);
#endif
    }
    unregister_event(arg);
}

static void handle_write_event(epoll_cb_arg * arg) {
    printf("%s: %p, epoll_cb_arg * arg %p\n", __func__, handle_read_event, arg->fun);
    if (arg->ei->pool) {
        task_arg *arg2Task = (task_arg *) malloc(sizeof(task_arg));
        arg2Task->ei = arg->ei;
        arg2Task->fd = arg->fd;
        sword::task task1((void *(*)(void *)) process_write_asyn, arg2Task);
        arg->ei->pool->commit_task(task1);
    } else {
#if 0
        process_write(arg->ei, arg->fd);
#else
        arg->ei->process_write(arg->fd);
#endif
    }
    unregister_event(arg);
}

epoll_oop_info::epoll_oop_info(int max_count, net_app * na, ev_way way, ...) {
    this->epoll_fd = epoll_create(1);
    this->timeout = -1;
    this->na = na;
    epoll_cb_arg *arg = build_epoll_cb_arg(this, na->listen_fd, handle_connection_event);
    this->root_eca = arg;
    register_event(arg, RD);
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
            ecv->fun(ecv);
        }
    }
}


void do_epoll_oop(void * arg) {
    printf("%s\n", __func__);
    net_app * na = (net_app *)arg;
#if USE_CONCURRENCY
    epoll_oop_info ei(FD_SETSIZE, na, ASYN, 8, 16, 16);
#else
    epoll_oop_info ei(&ei, FD_SETSIZE, na, SYN);
#endif
    ei.run();
}