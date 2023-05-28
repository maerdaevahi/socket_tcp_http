#include "concurrency.h"
#include "common_error.h"
#include <unistd.h>
#include <stdio.h>

static void communicate(const net_app *na, int client_fd);

void do_multiple_process(void * arg) {
#ifdef DEBUG
    printf("%s\n", __func__);
#endif
    net_app * na = (net_app *)arg;
    while (1) {
        int client_fd = na->handle_connection(na->listen_fd);
#ifdef USE_CONCURRENCY
        int pid = perror_exit_conditional(fork(), "fork");
        if (pid > 0) {
            perror_exit_conditional(close(client_fd), "close");
        } else if (!pid) {
            communicate(na, client_fd);
            break;
        }
#else
        communicate(na, client_fd);
#endif
    }
}

static void communicate(const net_app *na, int client_fd) {
#ifdef USE_CONCURRENCY
    perror_exit_conditional(close(na->listen_fd), "close");
#endif
    void * ctx = na->mk_context(client_fd);
    na->handle_read(ctx);
    na->handle_business(ctx);
    na->handle_write(ctx);
    na->free_context(ctx);
}

