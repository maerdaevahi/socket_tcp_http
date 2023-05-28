#ifndef CONCURRENCY
#define CONCURRENCY
typedef struct net_app {
    int listen_fd;
    int (*handle_connection)(int);
    void (*handle_read)(void *);
    void (*handle_business)(void *);
    void (*handle_write)(void *);
    void * (*mk_context)(int);
    void (*free_context)(void *);
} net_app;
typedef void (*invoke_concurrency)(void *);
void do_multiple_process(void *);
void do_multiple_thread(void *);
void do_select(void *);
void do_poll(void *);
void do_epoll(void *);
#endif