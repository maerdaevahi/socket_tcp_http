#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "concurrency.h"
#include "common_error.h"
#include <stdio.h>
#include <stdlib.h>
#include "thr/thread_pool.h"
#define CLIENT_FDS_SIZE (FD_SETSIZE - 4)

#define R 0
#define W 1

typedef struct task_arg {
    net_app *na;
    int fd;
} task_arg;

typedef struct select_info {
    fd_set readfds;
    fd_set readfds_io;
    fd_set writefds;
    fd_set writefds_io;
    int max_fd;
    int client_fds[CLIENT_FDS_SIZE];
    int max_pos;
    int ready_count;
    net_app * na;
    thread_pool * pool;
} select_info;

static void handle(task_arg * arg);

static void run(select_info * si);

static void init_select_info(select_info * si, net_app * na, int initial_threads, int max_threads, int max_tasks) {
    si->na = na;
    FD_ZERO(&si->readfds);
    FD_ZERO(&si->writefds);
    FD_SET(si->na->listen_fd, &si->readfds);
    si->max_fd = si->na->listen_fd;
    for (int i = 0; i < CLIENT_FDS_SIZE; ++i) {
        si->client_fds[i] = -1; 
    }
    si->max_pos = -1;
    si->pool = (thread_pool *)malloc(sizeof(thread_pool));
    init_thread_pool(si->pool, initial_threads, max_threads, max_tasks);
}

static void destroy_select_info(select_info * si) {
    if (si->pool) {
        destroy_thread_pool(si->pool);
        free(si->pool);
    }
}

static void add_fd(select_info * si, int client_fd, int rw) {
    switch (rw) {
        case R:
            FD_SET(client_fd, &si->readfds);
            break;
        case W:
            FD_SET(client_fd, &si->writefds);
            break;
    }
    if (si->max_fd < client_fd) {
        si->max_fd = client_fd;
    }
    int i;
    for (i = 0; i < CLIENT_FDS_SIZE; ++i) {
        if (si->client_fds[i] == -1) {
            si->client_fds[i] = client_fd;
            if (si->max_pos < i) {
                si->max_pos = i;
            }
            break;
        } 
    }
    if (i == CLIENT_FDS_SIZE) {
        fprintf(stderr, "too many client\n");
        exit(1);
    }
}

static int remove_fd(select_info * si, int i, int rw) {
    switch (rw) {
        case R:
            FD_CLR(si->client_fds[i], &si->readfds);
            break;
        case W:
            FD_CLR(si->client_fds[i], &si->writefds);
            break;
    }
    int fd = si->client_fds[i];
    si->client_fds[i] = -1;
    return fd;
}

static void process_listen_fd(select_info * si) {
    if (FD_ISSET(si->na->listen_fd, &si->readfds_io)) {
        --si->ready_count;
        int client_fd = si->na->handle_connection(si->na->listen_fd);
        add_fd(si, client_fd, R);
    }
}

static void process_client_fd(select_info * si) {
    for (int i = 0; si->ready_count > 0 && i <= si->max_pos; ++i) {
        if (si->client_fds[i] == -1) {
            continue;
        }
        if (FD_ISSET(si->client_fds[i], &si->readfds_io)) {
            --si->ready_count;
            task_arg *arg2Task = (task_arg *) malloc(sizeof(task_arg));
            arg2Task->na = si->na;
            arg2Task->fd = remove_fd(si, i, R);
            task task1;
            init_task(&task1, (void *(*)(void *)) handle, arg2Task);
            commit_task(si->pool, &task1);
            /*handle(arg2Task);*/
        }
    }
}

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

void do_select(void * arg)  {
    printf("%s\n", __func__);
    net_app * na = (net_app *)arg;
    select_info si;
    init_select_info(&si, na, 8, 16, 16);
    run(&si);
    destroy_select_info(&si);
}

static void run(select_info * si) {
    for (;;) {
        si->readfds_io = si->readfds;
        si->ready_count = select(si->max_fd + 1, &si->readfds_io, NULL, NULL, NULL);
        if (si->ready_count == -1) {
            perror("select");
            continue;
        } else if (!si->ready_count) {
            continue;
        } else if (si->ready_count > 0) {
            process_listen_fd(si);
            process_client_fd(si);
        } else {
            fprintf(stderr, "unknown error\n");
            break;
        }
    }
}
