#pragma once

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

/**
 * @brief TQueue takes and transfer ownership of the entries, once dequeded and 
 * out of scope the entry will be cleaned up.
 * 
 * Not an efficient implementation, not lock free.
 */
template <class T>
class TQueue
{
public:
    TQueue() : stop_flag(false)
    {
        fifo = std::make_unique<std::queue<std::unique_ptr<T>>>();
    }

    ~TQueue()
    {
        stop();
    }

    void stop()
    {
        if (stop_flag.load())
        {
            return;
        }
        stop_flag.store(true);
        has_entries.notify_all();
    }

    void enqueue(std::unique_ptr<T> entry)
    {
        std::lock_guard<std::mutex> lock(mutex);
        fifo->push(std::move(entry));
        has_entries.notify_one();
    }

    std::unique_ptr<T> dequeue() const
    {
        // take ownership of lock and block until an entry is pushed or shutdown
        std::unique_lock<std::mutex> lock(mutex);
        has_entries.wait(lock, [this]()
            {
                return !fifo->empty() || stop_flag.load();
            });

        if (stop_flag.load() && fifo->empty())
        {
            return std::unique_ptr<T>(nullptr);
        }
        std::unique_ptr<T> entry = std::move(fifo->front());
        fifo->pop();
        return entry;
    }

private:
    mutable std::mutex mutex;
    mutable std::condition_variable has_entries;
    std::atomic_bool stop_flag;
    std::unique_ptr<std::queue<std::unique_ptr<T>>> fifo;
};
