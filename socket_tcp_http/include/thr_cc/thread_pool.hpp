#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace sword {
template<typename T>
class blocking_queue {
    std::queue<T> container;
    std::mutex mtx;
    std::condition_variable not_full;
    std::condition_variable not_empty;
    int size;
public:
    blocking_queue(int size) : size(size) {
    }

    bool is_full() {
        return container.size() == size;
    }

    bool is_empty() {
        return container.empty();
    }

    void push(const T & t) {
        printf("%s\n",__func__);
        std::unique_lock<std::mutex> lk(mtx);
        while (is_full()) {
            not_full.wait(lk);
        }
        container.push(t);
        not_empty.notify_one();
        lk.unlock();
    }

    T pop() {
        std::unique_lock<std::mutex> lk(mtx);
        while (is_empty()) {
            not_empty.wait(lk);
        }
        printf("%s\n",__func__);
        T t = container.front();
        container.pop();
        not_full.notify_one();
        lk.unlock();
        return t;
    }
};

struct task {
    void * (*fun)(void *);
    void * arg;
    task(void * (*fun)(void *), void * arg) : fun(fun), arg(arg) {
    }
    void operator()() {
        fun(arg);
    }
};


template<typename tk = task>
class thread_pool {
    blocking_queue<tk> tasks;
public:
    thread_pool(int thread_number, int queue_size) : tasks(queue_size) {
        if (thread_number <= 0) {
            throw std::exception();
        }
        for (int i = 0; i < thread_number; ++i) {
            std::thread([&]() {
                for(;;) {
                    tk t = tasks.pop();
                    t();
                }
            }).detach();
        }
    }

    void commit_task(tk & t) {
        tasks.push(t);
    }
};
}


