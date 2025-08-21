#include "image_cache.h"

#include "unique_queue.hpp"
#include "image_data.h"

#include <algorithm>

ImageCache::ImageCache(unsigned int cache_size_bytes, std::shared_ptr<TQueue<ImageData>> image_queue)
{
    if (cache_size_bytes < MIN_CACHE_SIZE_BYTES)
    {
        this->cache_size_bytes = MIN_CACHE_SIZE_BYTES;
    }
    else
    {
        this->cache_size_bytes = std::min(cache_size_bytes, MAX_CACHE_SIZE_BYTES);
    }
    cache_thread = std::jthread{
        [image_queue, this](std::stop_token ctrl){
            run(ctrl, image_queue);
        }};
    this->not_found = std::make_unique<ImageData>();
}

void ImageCache::resize_by(int delta_size_bytes)
{
    if (delta_size_bytes < 1)
    {
        // no-op
        return;
    }
    // write lock the cache and update size
    const std::lock_guard<std::mutex> lock(write_mutex);
    cache_size_bytes += delta_size_bytes;
}

const ImageData& ImageCache::get(int key, bool& found_entry)
{
    found_entry = false;
    if (transient_dirty_locks.count(key))
    {
        while(transient_dirty_locks.count(key) && transient_dirty_locks[key] == DirtyFlag{true});
    }
    if (!cache_entries.count(key) || !cache_entries[key].get())
    {
        return *this->not_found.get();
    }
    found_entry = true;
    return *cache_entries[key].get();
}

void ImageCache::run(std::stop_token ctrl, std::shared_ptr<TQueue<ImageData>> result_queue)
{
    // atomic dirty flag per entry - transient
    while (!ctrl.stop_requested())
    {
        std::unique_ptr<ImageData> image_data = result_queue->dequeue();
        if (image_data)
        {
            int key = image_data->key();
            transient_dirty_locks.emplace(key, DirtyFlag{std::atomic<bool>{true}});
            std::lock_guard<std::mutex> lock(write_mutex);
            if (running_size_bytes + image_data->size_bytes > cache_size_bytes)
            {
                bust_least_used_cache(image_data->size_bytes);
            }
            running_size_bytes += image_data->size_bytes;
            used_count.insert(std::pair<int, AtomicWrapper<int>>(key, AtomicWrapper<int>{std::atomic<int>{0}}));
            // transfer ownership once done accessing image_data
            cache_entries.insert(std::pair<int, std::unique_ptr<ImageData>>(key, std::move(image_data)));
            transient_dirty_locks.erase(key);
        }
    }
}

void ImageCache::bust_least_used_cache(int required_free_bytes)
{
    // TODO: use min priority queue instead
    // check size and keep busting until enough space is left for incoming image
    if (cache_size_bytes - running_size_bytes > required_free_bytes)
    {
        return;
    }
    std::vector<int> cands;
    for (const auto& pair : used_count)
    {
        if (pair.second.load() == 0)
        {
            cands.push_back(pair.first);
        }
    }

    for (const int cand : cands)
    {
        {
            used_count.erase(cand);
            int busted_size = cache_entries[cand]->size_bytes;
            cache_entries.erase(cand);
            running_size_bytes -= busted_size;
        }
        if (cache_size_bytes - running_size_bytes > required_free_bytes)
        {
            break;
        }
    }
}
