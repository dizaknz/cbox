#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

struct TaskStatus;
struct ImageData;
template <class T> class TQueue;
class IImageDataReader;
class ImageDataResizer;

/**
 * Args for requesting an image
 */
struct ImageTask
{
    std::string task_id;
    std::unique_ptr<IImageDataReader> reader;
    std::unique_ptr<ImageDataResizer> resizer;
    bool auto_resize;
    int desired_width;
    int desired_height;
    std::shared_ptr<TQueue<TaskStatus>> status_queue;
    std::shared_ptr<TQueue<ImageData>> result_queue;
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
    void submit_task(ImageTask *const task);

private:
    std::vector<std::unique_ptr<ImageTaskRunner>> runner_pool;
    std::shared_ptr<TQueue<ImageTask>> task_queue;
    std::mutex pool_mutex;
    unsigned int pool_size;
};
