#include "image_manager.h"
#include "basic_queue.hpp"
#include "image_data.h"
#include "image_cache.h"
#include "image_task_manager.h"

#include <future>

class ImageManager::Impl : public std::enable_shared_from_this<ImageManager::Impl>
{
public:
    Impl(int image_cache_size_mb, int task_pool_size, int request_timeout_ms) : request_timeout_ms(request_timeout_ms)
    {
        // image queue transfers ownership of raw data
        image_queue = std::make_shared<TQueue<std::unique_ptr<ImageData>>>();
        image_cache = std::make_unique<ImageCache>(image_cache_size_mb << 20, image_queue);
        task_manager = std::make_unique<ImageTaskManager>(task_pool_size);
    }
    ~Impl()
    {
        image_queue->shutdown();
    }

    void request_image_load(const std::string& image_path, std::shared_ptr<IImageHandler> handler)
    {
        std::weak_ptr<Impl> weak_impl = shared_from_this();
        // run on same thread
        std::future done = std::async(std::launch::deferred, [weak_impl, image_path, handler]() {
            if (auto impl = weak_impl.lock())
            {
                // query cache
                bool found_entry = false;
                int key = get_image_key(image_path);
                const ImageData &image_data = impl->image_cache->get(key, found_entry);
                if (found_entry)
                {
                    handler->Process(image_data);
                    return;
                }
                
                // if not in cache, submit request to load
                std::string task_id = "image loader: " + std::to_string(key);
                std::string file = std::string(image_path);
                impl->task_manager->submit_task(ImageTask(
                    task_id,
                    file,
                    impl->task_status_queue,
                    impl->image_queue));

                int wait_ms = 0;
                found_entry = false;
                while(!found_entry)
                {
                    auto& check = impl->image_cache->get(key, found_entry);
                    if (wait_ms > impl->request_timeout_ms)
                    {
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
                    wait_ms++;
                }
                handler->Process(impl->image_cache->get(key, found_entry));
            }
        });
    }

    void request_image_resize(const std::string& image_path, int size_x, int size_y, std::shared_ptr<IImageHandler> handler)
    {
        std::weak_ptr<Impl> weak_impl = shared_from_this();
        // run on same thread
        std::future done = std::async(std::launch::deferred, [weak_impl, image_path, size_x, size_y, handler]() {
            if (auto impl = weak_impl.lock())
            {
                // query cache for resized image, if not in cache submit request to resize
                bool found_entry = false;
                bool is_original_size = false;
                int key = get_image_key(image_path, size_x, size_y, is_original_size);
                const ImageData &image_data = impl->image_cache->get(key, found_entry);
                if (found_entry)
                {
                    handler->Process(image_data);
                    return;
                }
                std::string task_id = "image resizer: " + std::to_string(key);
                std::string file = std::string(image_path);
                impl->task_manager->submit_task(ImageTask(
                    task_id,
                    file,
                    size_x,
                    size_y,
                    impl->task_status_queue,
                    impl->image_queue));

                int wait_ms = 0;
                found_entry = false;
                while(!found_entry)
                {
                    auto& check = impl->image_cache->get(key, found_entry);
                    if (wait_ms > impl->request_timeout_ms)
                    {
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
                    wait_ms++;
                }
                handler->Process(impl->image_cache->get(key, found_entry));
            }
        });
        done.wait();
    }

    void register_task_status_queue(std::shared_ptr<TQueue<TaskStatus>> queue)
    {
        this->task_status_queue = queue;
    }
public:
    // impl is hidden, therefore public access
    std::shared_ptr<TQueue<std::unique_ptr<ImageData>>> image_queue;
    std::unique_ptr<ImageCache> image_cache;
    std::unique_ptr<ImageTaskManager> task_manager;
    std::shared_ptr<TQueue<TaskStatus>> task_status_queue;
    int request_timeout_ms;
};

ImageManager::ImageManager(int image_cache_size_mb, int task_pool_size, int request_timeout_ms)
{
    impl = std::make_shared<Impl>(image_cache_size_mb, task_pool_size, request_timeout_ms);
}

void ImageManager::request_image_load(
    const std::string& image_path,
    std::shared_ptr<IImageHandler> handler)
{
    impl->request_image_load(image_path, handler);
}

void ImageManager::request_image_resize(
    const std::string& image_path,
    int size_x,
    int size_y,
    std::shared_ptr<IImageHandler> handler)
{
    impl->request_image_resize(image_path, size_x, size_y, handler);
}

void ImageManager::register_task_status_queue(std::shared_ptr<TQueue<TaskStatus>> queue)
{
    impl->register_task_status_queue(queue);
}

ImageManager::~ImageManager()
{
    impl.reset();
}
