#ifndef __THREAD__
#define __THREAD__
#include <pthread.h>
typedef struct thread {
    pthread_t thread_id;
} thread;

void assign_thread(thread * t, thread * t1);
#endif

/* PTHREAD_MUTEX_TIMED_NP：

此类型的互斥量可以使用pthread_mutex_timedlock()函数来获取，它允许指定一个超时时间，如果在指定的时间内没有获取到互斥量，则返回错误。

PTHREAD_MUTEX_RECURSIVE_NP：

此类型的互斥量允许同一个线程对同一个互斥量反复加锁，而不会引起死锁。

PTHREAD_MUTEX_ERRORCHECK_NP：

此类型的互斥量在检测到死锁时会返回错误，以提示程序员发现死锁的位置。

PTHREAD_MUTEX_ADAPTIVE_NP：

此类型的互斥量可以根据系统的负载情况自动调整，以提高系统的效率。 */