#include <iostream>
#include "base_socket.h"
#define ANY_IP "0.0.0.0"
#define DEFAULT_PORT 10000
#include "concurrency.h"
#include "http.h"
#include <unistd.h>
#define PROCESS 0
#define THREAD 1
#define SELECT 2
#define POLL 3
#define EPOLL 4

invoke_concurrency ic[5] = {do_multiple_process, do_multiple_thread, do_select, NULL, do_epoll};

invoke_concurrency determine_concurrency_way(int argc, char ** argv);

int main(int argc, char ** const argv, char ** const envp) {
    setbuf(stdout, NULL);
    chdir("../..");
    int listen_fd = open_nonblock_ipv4_tcp_listen_socket(ANY_IP, DEFAULT_PORT);
    net_app na;
    na.listen_fd = listen_fd;
    na.handle_connection = accept_connection;
    na.handle_read = (void (*)(void *))parse_http_request;
    na.handle_business= (void (*)(void *))service;
    na.handle_write = (void (*)(void *))encode;
    na.mk_context = mk_http_context;
    na.free_context = free_http_context;
    invoke_concurrency ivk = determine_concurrency_way(argc, argv);
    ivk(&na);
    return 0;
}

invoke_concurrency determine_concurrency_way(int argc, char ** argv) {
    int way;
    if (argc == 1) {
        way = PROCESS;
    } else if (argc == 2) {
        way = atoi(argv[1]);
        if (way < PROCESS || way > EPOLL) {
            way = PROCESS;
        }
    }  else {
        fprintf(stderr, "to many arguments\n");
        exit(1);
    }
    return ic[way];
}
