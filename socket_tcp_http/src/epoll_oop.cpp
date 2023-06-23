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

typedef struct task_arg {
    void * ei;
    int fd;
} task_arg;

class epoll_oop_info {
    int epoll_fd;
    std::mutex mutex;
    epoll_cb_arg * root_eca;
    struct epoll_event * events;
    int max_count;
    int ready_count;
    int timeout;
    sword::thread_pool<sword::task> * pool;
    net_app * na;
public:
    epoll_oop_info(int max_count, net_app * na, ev_way way, ...);
    ~epoll_oop_info();
    void run();
    int handle_connection(int fd);
    void process_read(int fd);
    void process_write(int fd);
    void register_event(int fd, ev_tp et, void * eca);
    void unregister_event(int fd);
    bool is_asyn_supported();
    void commit_asyn(int fd, ev_tp et);
    static void process_read_asyn(task_arg * arg);
    static void process_write_asyn(task_arg * arg);
};

int epoll_oop_info::handle_connection(int fd) {
    int peer_fd = this->na->handle_connection(fd);
    if (peer_fd != -1) {
        void *ctx = this->na->mk_context(peer_fd);
        if (ctx_set[peer_fd]) {
            fprintf(stderr, "resource is not clean\n");
            exit(1);
        } else {
            ctx_set[peer_fd] = ctx;
        }
    }
    return peer_fd;
}

bool epoll_oop_info::is_asyn_supported() {
    return this->pool;
}

void epoll_oop_info::commit_asyn(int fd, ev_tp et) {
    task_arg *arg2Task = (task_arg *) malloc(sizeof(task_arg));
    arg2Task->ei = this;
    arg2Task->fd = fd;
    sword::task task1((void *(*)(void *)) (et == RD ? epoll_oop_info::process_read_asyn : epoll_oop_info::process_write_asyn), arg2Task);
    this->pool->commit_task(task1);
}

class epoll_cb_arg {
    int fd;
    epoll_oop_info * ei;
    void (epoll_cb_arg::*mem_fun)();
public:
    epoll_cb_arg() = default;
    epoll_cb_arg(epoll_oop_info * ei, int peer_fd, void (epoll_cb_arg::*fun)());
    void handle_read_event();
    void handle_write_event();
    void handle_connection_event();
    void operator()() {
        (this->*mem_fun)();
    }
};

epoll_cb_arg::epoll_cb_arg(epoll_oop_info * ei, int peer_fd, void (epoll_cb_arg::*fun)()) {
    this->mem_fun = fun;
    this->ei = ei;
    this->fd = peer_fd;
}

void epoll_oop_info::register_event(int fd, ev_tp et, void * eca) {
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
    ev.data.ptr = eca;
    std::mutex * mtx = &this->mutex;
    mtx->lock();
    epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    mtx->unlock();
}

void epoll_oop_info::unregister_event(int fd) {
    std::mutex * mtx = &this->mutex;
    mtx->lock();
    epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    mtx->unlock();
    //free(this);
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
    register_event(fd, WR, read_arg);
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

void epoll_oop_info::process_read_asyn(task_arg * arg) {
    epoll_oop_info * na = (epoll_oop_info *)arg->ei;
    int fd = arg->fd;
    na->process_read(fd);
    free(arg);
}

void epoll_oop_info::process_write_asyn(task_arg * arg) {
    epoll_oop_info * na = (epoll_oop_info *)arg->ei;
    int fd = arg->fd;
    na->process_write(fd);
    free(arg);
}

void epoll_cb_arg::handle_read_event() {
    if (this->ei->is_asyn_supported()) {
        this->ei->commit_asyn(this->fd, RD);
    } else {
        this->ei->process_read(this->fd);
    }
    this->ei->unregister_event(this->fd);
    delete this;
}

void epoll_cb_arg::handle_write_event() {
    if (this->ei->is_asyn_supported()) {
        this->ei->commit_asyn(this->fd, WR);
    } else {
        this->ei->process_write(this->fd);
    }
    this->ei->unregister_event(this->fd);
    delete this;
}

void epoll_cb_arg::handle_connection_event() {
    while (1) {
        int peer_fd = this->ei->handle_connection(this->fd);
        if (peer_fd == -1) {
            perror(__func__);
            break;
        }
        epoll_cb_arg * read_arg = new epoll_cb_arg(this->ei, peer_fd, &epoll_cb_arg::handle_read_event);
        read_arg->ei->register_event(peer_fd, RD, read_arg);
    }
}

epoll_oop_info::epoll_oop_info(int max_count, net_app * na, ev_way way, ...) {
    this->epoll_fd = epoll_create(1);
    this->timeout = -1;
    this->na = na;
    this->root_eca = new epoll_cb_arg(this, na->listen_fd, &epoll_cb_arg::handle_connection_event);
    this->register_event(na->listen_fd, RD, this->root_eca);
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
            (*ecv)();
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