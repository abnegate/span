#include "thread_pool.h"

namespace dev {
    ThreadPool::ThreadPool(size_t maxThreads) : shutdown_(false) {
        if (maxThreads == 0) {
            maxThreads = std::thread::hardware_concurrency();
        }
        for (size_t i = 0; i < maxThreads; ++i) {
            workers_.emplace_back([this] { this->workerFunction(); });
        }
    }

    ThreadPool::~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            shutdown_ = true;
        }
        condition_.notify_all();
        for (std::thread& worker : workers_) {
            worker.join();
        }
    }

    void ThreadPool::workerFunction() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this] {
                    return this->shutdown_ || !this->tasks_.empty();
                });
                if (this->shutdown_ && this->tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }
}
