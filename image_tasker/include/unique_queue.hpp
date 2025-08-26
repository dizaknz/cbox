#pragma once

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <thread>

/**
 * @brief Basic threadsafe queue
 */
template <class T>
class TQueue
{
public:
    TQueue() : queue(std::make_unique<std::queue<T>>()), shutdown_flag(false)
    {}

    ~TQueue()
    {
        shutdown();
    }

    void shutdown()
    {
        if (shutdown_flag.load())
        {
            return;
        }
        shutdown_flag.store(true);
        has_entries.notify_all();
    }

    void enqueue(T&& entry)
    {
        std::lock_guard<std::mutex> lock(mutex);
        queue->push(std::move(entry));
        has_entries.notify_one();
    }

    std::optional<T> dequeue()
    {
        std::optional<T> entry;
        std::lock_guard<std::mutex> lock(mutex);
        if (!queue->empty())
        {
            entry.emplace(std::move(queue->front()));
            queue->pop();
        }
        return entry;
    }

    void wait_for_entry_or_shutdown() const
    {
        std::unique_lock<std::mutex> lock(mutex);
        has_entries.wait(lock, [this]()
        {
            return !queue->empty() || shutdown_flag.load();
        });
    }

    void wait_for_entry(std::stop_token stop) const
    {
        std::unique_lock<std::mutex> lock(mutex);
        has_entries.wait(lock, stop, [this]()
        {
            return !queue->empty();
        });
    }
private:
    mutable std::mutex mutex;
    mutable std::condition_variable has_entries;
    std::atomic_bool shutdown_flag;
    std::unique_ptr<std::queue<T>> queue;
};
