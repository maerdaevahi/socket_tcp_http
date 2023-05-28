#include "concurrency.h"
#include <unistd.h>
#include <pthread.h>
#include <malloc.h>
typedef struct thread_arg {
    net_app *na;
    int client_fd;
} thread_arg;
static void * communicate(void *);

void do_multiple_thread(void * arg) {
#ifdef DEBUG
    printf("%s\n", __func__);
#endif
    net_app * na = (net_app *)arg;
    pthread_t newthread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    while (1) {
        int client_fd = na->handle_connection(na->listen_fd);
        thread_arg * ta = (thread_arg *) malloc(sizeof(thread_arg));
        ta->na = na;
        ta->client_fd = client_fd;
#ifdef USE_CONCURRENCY
        pthread_create(&newthread, &attr, communicate, ta);

#else
        communicate(ta);
#endif
    }
    pthread_attr_destroy(&attr);
}

static void * communicate(void * arg) {
    if (!arg) {
        return NULL;
    }
    thread_arg * ta = (thread_arg *)arg;
    net_app * na = ta->na;
    int client_fd = ta->client_fd;
    void * ctx = na->mk_context(client_fd);
    na->handle_read(ctx);
    na->handle_business(ctx);
    na->handle_write(ctx);
    na->free_context(ctx);
    free(arg);
    return NULL;
}

