#include <iostream>
#include "base_socket.h"
#define ANY_IP "0.0.0.0"
#define DEFAULT_PORT 10000
#include "concurrency.h"
#include "http.h"
#include <unistd.h>
#include "config.h"
#define PROCESS 0
#define THREAD 1
#define SELECT 2
#define POLL 3
#define EPOLL 4
#define EPOLL_OOP 5
#define CONFIG_PATHNAME "../../static/server_config.json"

invoke_concurrency ic[6] = {do_multiple_process, do_multiple_thread, do_select, NULL, do_epoll, do_epoll_oop};


int get_way(int argc, char *const *argv, int way);

int main(int argc, char ** const argv, char ** const envp) {
    setbuf(stdout, NULL);
    int way = get_way(argc, argv, way);
    server_config * cfg = parse_config(CONFIG_PATHNAME);
    int listen_fd;
    if (!cfg) {
        chdir("../..");
        if (way == EPOLL || way == EPOLL_OOP) {
            listen_fd = open_nonblock_ipv4_tcp_listen_socket(ANY_IP, DEFAULT_PORT);
        } else {
            listen_fd = open_ipv4_tcp_listen_socket(ANY_IP, DEFAULT_PORT);
        }
    } else {
        chdir(cfg->cwd ? cfg->cwd : "../..");
        if (way == EPOLL || way == EPOLL_OOP) {
            listen_fd = open_nonblock_ipv4_tcp_listen_socket(cfg->ip, cfg->port);
        } else {
            listen_fd = open_ipv4_tcp_listen_socket(cfg->ip, cfg->port);
        }
    }

    net_app na;
    na.listen_fd = listen_fd;
    na.handle_connection = accept_connection;
    na.handle_read = (void (*)(void *))parse_http_request;
    na.handle_business= (void (*)(void *))service;
    na.handle_write = (void (*)(void *))encode;
    na.mk_context = mk_http_context;
    na.free_context = free_http_context;
    invoke_concurrency ivk = ic[way];
    ivk(&na);
    free(cfg);
    return 0;
}

int get_way(int argc, char *const *argv, int way) {
    if (argc == 1) {
        way = PROCESS;
    } else if (argc == 2) {
        way = atoi(argv[1]);
        if (way < PROCESS || way > EPOLL_OOP) {
            way = PROCESS;
        }
    }  else {
        fprintf(stderr, "to many arguments\n");
        exit(1);
    }
    return way;
}
