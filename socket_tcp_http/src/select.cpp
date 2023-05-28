#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "concurrency.h"
#include "common_error.h"
#include <stdio.h>
#include <stdlib.h>
// void FD_CLR(int fd, fd_set *set);
// int  FD_ISSET(int fd, fd_set *set);
// void FD_SET(int fd, fd_set *set);
// void FD_ZERO(fd_set *set);
#define CLIENT_FDS_SIZE (FD_SETSIZE - 4)

typedef struct select_info {
    fd_set readfds;
    int max_fd;
    int client_fds[CLIENT_FDS_SIZE];
    int max_pos;
    net_app * na;
} select_info;

void init_select_info(select_info * si, net_app * na) {
    si->na = na;
    FD_ZERO(&si->readfds);
    FD_SET(si->na->listen_fd, &si->readfds);
    si->max_fd = si->na->listen_fd;
    for (int i = 0; i < CLIENT_FDS_SIZE; ++i) {
        si->client_fds[i] = -1; 
    }
    si->max_pos = -1;
}

void add_select_info(select_info * si, int client_fd) {
    FD_SET(client_fd, &si->readfds);
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

void remove_select_info(select_info * si, int i) {
    close(si->client_fds[i]);
    FD_CLR(si->client_fds[i], &si->readfds);
    si->client_fds[i] = -1;
}

int process_listen_fd_if_necessary(select_info * si, fd_set * readfds_param, int nready) {
    if (FD_ISSET(si->na->listen_fd, readfds_param)) {
        int client_fd = si->na->handle_connection(si->na->listen_fd);
        add_select_info(si, client_fd);
        --nready;
    }
    return nready;
}

void process_client_fd_if_necessary(select_info * si, fd_set * readfds_param, int nready) {
    for (int i = 0; nready > 0 && i <= si->max_pos; ++i) {
        if (si->client_fds[i] == -1) {
            continue;
        }
        if (FD_ISSET(si->client_fds[i], readfds_param)) {
            net_app * na = si->na;
            void * ctx = na->mk_context(si->client_fds[i]);
            na->handle_read(ctx);
            na->handle_business(ctx);
            na->handle_write(ctx);
            na->free_context(ctx);
            remove_select_info(si, i);
        }
        --nready;       
    }
}

void do_select(void * arg)  {
    printf("%s\n", __func__);
    net_app * proto = (net_app *)arg;
    select_info si;
    init_select_info(&si, proto);
    for (;;) {
        fd_set readfds_param = si.readfds;
        int nready = select(si.max_fd + 1, &readfds_param, NULL, NULL, NULL);
        if (nready == -1) {
            perror("select");
            continue;
        } else if (!nready) {
            continue;
        } else if (nready > 0) {
            nready = process_listen_fd_if_necessary(&si, &readfds_param, nready);
            process_client_fd_if_necessary(&si, &readfds_param, nready);
        } else {
            fprintf(stderr, "unknown error\n");
            exit(1);
        }
    } 
}