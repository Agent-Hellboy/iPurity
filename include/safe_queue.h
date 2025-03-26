// SafeQueue.h
#ifndef SAFE_QUEUE_H
#define SAFE_QUEUE_H
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

template <typename T>
class SafeQueue {
public:
    void push(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(value);
        cond_.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [&]() { return !queue_.empty() || done_; });

        if (queue_.empty()) return std::nullopt;

        T value = queue_.front();
        queue_.pop();
        return value;
    }

    void set_done() {
        std::lock_guard<std::mutex> lock(mutex_);
        done_ = true;
        cond_.notify_all();
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool done_ = false;
};

#endif