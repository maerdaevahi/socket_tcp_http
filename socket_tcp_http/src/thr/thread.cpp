#include "thr/thread.h"
void assign_thread(thread * t, thread * t1) {
    t->thread_id = t1->thread_id;
}