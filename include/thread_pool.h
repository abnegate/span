#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

namespace dev {
    class ThreadPool {
    public:
        explicit ThreadPool(size_t maxThreads);
        ~ThreadPool();

        template<class F>
        auto enqueue(F&& f) -> std::future<decltype(f())>;

    private:
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex queue_mutex_;
        std::condition_variable condition_;
        bool shutdown_;

        void workerFunction();
    };

    template<class F>
    auto ThreadPool::enqueue(F&& f) -> std::future<decltype(f())> {
        auto task = std::make_shared<std::packaged_task<decltype(f())()>>(std::forward<F>(f));
        std::future<decltype(f())> res = task->get_future();

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (shutdown_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return res;
    }
}
