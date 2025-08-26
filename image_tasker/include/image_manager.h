#pragma once

#include <string>
#include <memory>

class ImageData;
template <class T> class TQueue;
struct TaskStatus;

class IImageHandler
{
public:
    virtual void Process(const ImageData&) const = 0;
};

/**
 * ImageManager encapsulates the image cache and image task system to support
 * async loading and resizing of images from disk or memory.
 * 
 */
class ImageManager
{
public:
    ImageManager() = delete;
    ImageManager(const ImageManager&) = delete;
    ImageManager(int image_cache_size_mb, int task_pool_size, int request_timeout_ms);
    ~ImageManager();

    /**
     * @brief Request loading an image from disk or memory for the image handler provided.
     */
    void request_image_load(
        const std::string& image_path,
        std::shared_ptr<IImageHandler> handler);

    /**
     * @brief Request resizing an image to be processed by the provided image handler.
     */
    void request_image_resize(
        const std::string& image_path,
        int size_x,
        int size_y,
        std::shared_ptr<IImageHandler> handler);

    void register_task_status_queue(std::shared_ptr<TQueue<TaskStatus>> queue);
private:
    /**
     * @brief shared for async
     */
    class Impl; std::shared_ptr<Impl> impl;
};
