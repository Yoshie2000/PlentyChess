#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <queue>
#include <stdexcept>
#include <thread>

class SfThreadPool {
   public:
    SfThreadPool(std::size_t num_threads) : stop_(false) {
        for (std::size_t i = 0; i < num_threads; ++i) workers_.emplace_back([this] { work(); });
    }

    template <class F, class... Args>
    void enqueue(F &&func, Args &&...args) {
        auto task = std::make_shared<std::packaged_task<void()>>(
            std::bind(std::forward<F>(func), std::forward<Args>(args)...));

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) throw std::runtime_error("Warning: enqueue on stopped ThreadPool");
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
    }

    ~SfThreadPool() { wait(); }

    void wait() {
        if (stop_) return;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }

        condition_.notify_all();

        for (auto &worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

   private:
    void work() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;

    std::atomic_bool stop_;
};
