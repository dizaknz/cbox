#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <map>
#include <atomic>

template <class T> class TQueue;
struct ImageData;

template <class T>
struct AtomicWrapper
{
    std::atomic<T> atomic_value;

    AtomicWrapper() : atomic_value()
    {}
    
    AtomicWrapper(const std::atomic<T> &in_value)
    : atomic_value(in_value.load())
    {}

    AtomicWrapper(const AtomicWrapper& other)
    : atomic_value(other.atomic_value.load())
    {}

    AtomicWrapper &operator=(const AtomicWrapper &other)
    {
        atomic_value.store(other.atomic_value.load());
    }

    bool operator==(const AtomicWrapper& other) const
    {
        return atomic_value.load() == other.atomic_value.load();
    }

    T load() const
    {
        return atomic_value.load();
    }

    void store(T val)
    {
        atomic_value.store(val);
    }
};

using DirtyFlag = AtomicWrapper<bool>;

constexpr unsigned int MAX_CACHE_SIZE_BYTES = 1024 << 20;

/**
 * ImageCache manages the in-memory cache.
 * 
 * - consumes task results from queue and take ownership
 * - caches result by their unique key
 * - if cache is full find least used and bust their cache
 * - emits metrics to a shared queue
 */
class ImageCache
{
public:
    ImageCache() = delete;
    ImageCache(const ImageCache&) = delete;
    ImageCache(unsigned int cache_size_bytes, std::shared_ptr<TQueue<ImageData>> image_queue);
    void resize_by(int delta_size_bytes);
    const ImageData& get(const std::string& image_file_name, int width, int height, bool& found_entry);

private:
    /**
     * @brief run consumes of the image queue and populates cache
     */
    void run(std::stop_token ctrl, std::shared_ptr<TQueue<ImageData>> result_queue);

    /**
     * @brief Keep busting least used cache entries until the required space has been freed up
     */
    void bust_least_used_cache(int required_free_bytes);

private:
    unsigned int cache_size_bytes = 0;
    unsigned int running_size_bytes = 0;
    std::jthread cache_thread;
    std::unordered_map<int, DirtyFlag> transient_dirty_locks;
    std::mutex write_mutex;
    std::unordered_map<int, std::unique_ptr<ImageData>> cache_entries;
    std::map<int, AtomicWrapper<int>> used_count;
    std::unique_ptr<ImageData> not_found;
};