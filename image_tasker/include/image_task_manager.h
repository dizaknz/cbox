#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

#include "image_data.h"

struct TaskStatus;
template <class T> class TQueue;

/**
 * Args for requesting an image
 */
struct ImageTask
{
    std::string task_id;
    std::unique_ptr<IImageDataReader> reader;
    std::unique_ptr<ImageDataResizer> resizer;
    bool auto_resize;
    std::shared_ptr<TQueue<TaskStatus>> status_queue;
    std::shared_ptr<TQueue<std::unique_ptr<ImageData>>> result_queue;

    ImageTask() = delete;
    ImageTask(const ImageTask&) = delete;
    ImageTask(
        const std::string& task_id,
        const std::string& source_file,
        std::shared_ptr<TQueue<TaskStatus>> status_queue,
        std::shared_ptr<TQueue<std::unique_ptr<ImageData>>> result_queue)
    : task_id(task_id),
      reader(std::make_unique<ImageDiskReader>(source_file)),
      auto_resize(false),
      status_queue(status_queue),
      result_queue(result_queue)
    {}
    ImageTask(
        const std::string& task_id,
        const std::string& source_file,
        int size_x,
        int size_y,
        std::shared_ptr<TQueue<TaskStatus>> status_queue,
        std::shared_ptr<TQueue<std::unique_ptr<ImageData>>> result_queue)
    : task_id(task_id),
      reader(std::make_unique<ImageDiskReader>(source_file)),
      resizer(std::make_unique<ImageDataResizer>(size_x, size_y)),
      auto_resize(true),
      status_queue(status_queue),
      result_queue(result_queue)
    {}
    ImageTask(ImageTask&& other) noexcept
    : task_id(std::move(other.task_id)), 
      reader(std::move(other.reader)),
      resizer(std::move(other.resizer)),
      auto_resize(other.auto_resize),
      status_queue(std::move(other.status_queue)),
      result_queue(std::move(other.result_queue))
    {}
 };

/**
 * Interruptable task runner
 */
class ImageTaskRunner
{
public:
    ImageTaskRunner() = delete;
    ImageTaskRunner(std::shared_ptr<TQueue<ImageTask>> task_queue);
    ImageTaskRunner(const ImageTaskRunner&) = delete;
    ~ImageTaskRunner();

private:
    void run(std::stop_token ctrl, std::shared_ptr<TQueue<ImageTask>> task_queue);
    void stop();
    void report_task_failure(const ImageTask& task, std::chrono::duration<double> duration, std::vector<std::string> errors);
    void report_task_success(const ImageTask& task, std::chrono::duration<double> duration);


private:
    std::jthread runner_thread;
    std::atomic_flag task_running = ATOMIC_FLAG_INIT;
};

constexpr unsigned int DEFAULT_POOL_SIZE = 8;

class ImageTaskManager
{
public:
    ImageTaskManager();
    ImageTaskManager(unsigned int pool_size);
    ~ImageTaskManager();
    void increase_pool_size_by(unsigned int delta_pool_size);
    void submit_task(ImageTask&& task);

private:
    std::vector<std::unique_ptr<ImageTaskRunner>> runner_pool;
    std::shared_ptr<TQueue<ImageTask>> task_queue;
    std::mutex pool_mutex;
    unsigned int pool_size;
};
