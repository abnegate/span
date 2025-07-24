#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace span::threads {
    class ThreadPool {
    public:
        explicit ThreadPool(const size_t numThreads) {
            for (size_t i = 0; i < numThreads; ++i) {
                workers.emplace_back([this] {
                    while (true) {
                        std::function<void()> task;
                        {
                            std::unique_lock lock(queueMutex);

                            condition.wait(lock, [this] {
                                return stop.load(std::memory_order_acquire) || !tasks.empty();
                            });

                            if (stop.load(std::memory_order_acquire) && tasks.empty()) {
                                return;
                            }

                            task = std::move(tasks.front());
                            tasks.pop();
                        }

                        try {
                            task();
                        } catch (const std::exception &e) {
                            // Log the exception?
                        } catch (...) {
                            // Catch any non-standard exceptions
                        }
                    }
                });
            }
        }

        ~ThreadPool() {
            {
                std::unique_lock lock(queueMutex);
                stop.store(true, std::memory_order_release);
            }

            condition.notify_all();

            for (std::thread &worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }

        template <class F, class... Args>
        std::future<std::invoke_result_t<F, Args...>> enqueue(
            F &&f,
            Args &&...args
        ) {
            using ReturnType = std::invoke_result_t<F, Args...>;

            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            std::future<ReturnType> result = task->get_future();
            {
                std::unique_lock lock(queueMutex);

                if (stop.load(std::memory_order_acquire)) {
                    throw std::runtime_error("Thread pool has been stopped.");
                }

                tasks.emplace([task] { (*task)(); });
            }

            condition.notify_one();
            return result;
        }

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queueMutex;
        std::condition_variable condition;
        std::atomic<bool> stop{false};
    };
} // namespace span::threads
