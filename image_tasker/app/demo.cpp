#include <iostream>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <filesystem>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "image_manager.h"

#include "image_data.h"
#include "task_status.h"
#include "unique_queue.hpp"
#include "image_task_manager.h"

using namespace spdlog;

// TODO: implement metrics queue
struct CacheMetrics
{
    unsigned int cache_hits;
    unsigned int cache_misses;
    unsigned int free_size_bytes;
    unsigned int cache_size_bytes;
};

class CacheMetricsReporter
{
public:
    CacheMetricsReporter() = delete;
    CacheMetricsReporter(std::shared_ptr<TQueue<CacheMetrics>> metrics_queue)
    {
        reporter_thread = std::jthread{
            [metrics_queue, this](std::stop_token ctrl){
                run(ctrl, metrics_queue);
            }};
    }

private:
    void run(std::stop_token ctrl, std::shared_ptr<TQueue<CacheMetrics>> metrics_queue)
    {
        if (!metrics_queue)
        {
            // quit
            return;
        }
        while (!ctrl.stop_requested())
        {
            std::unique_ptr<CacheMetrics> cache_metrics = metrics_queue->dequeue();
            if (cache_metrics)
            {
                info("Cache Metrics - hits: " + std::to_string(cache_metrics->cache_hits)
                     + " misses: " + std::to_string(cache_metrics->cache_misses)
                     + " free size: " + std::to_string(cache_metrics->free_size_bytes >> 20) + "MB "
                     + " cache size:" + std::to_string(cache_metrics->cache_size_bytes >> 20) + "MB");
            }
        }
    }

private:
    std::jthread reporter_thread;
};

class TaskStatusReporter
{
public:
    TaskStatusReporter() = delete;
    TaskStatusReporter(std::shared_ptr<TQueue<TaskStatus>> status_queue)
    {
        reporter_thread = std::jthread{
            [status_queue, this](std::stop_token ctrl){
                run(ctrl, status_queue);
            }};
    }

private:
    void run(std::stop_token ctrl, std::shared_ptr<TQueue<TaskStatus>> status_queue)
    {
        if (!status_queue)
        {
            // quit
            return;
        }
        while (!ctrl.stop_requested())
        {
            std::unique_ptr<TaskStatus> task_status = status_queue->dequeue();
            if (task_status)
            {
                auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(task_status->duration);
                info("Task Status - task: " + task_status->task_id
                     + " state: " + task_state_to_string(task_status->state)
                     + (task_status->state == TaskState::Failed ? " error: " + task_status->errors[0] : "")
                     + " time elapsed: " + std::to_string(elapsed_time.count()) + "ms");
            }
        }
    }

private:
    std::jthread reporter_thread;
};

class TestImageHandler : public IImageHandler
{
    void Process(const ImageData& image_data)
    {
        info("Processing image: " + image_data.source_file_path
             + " Width: " + std::to_string(image_data.width)
             + " Height: " + std::to_string(image_data.height));
    }
};

int main(int argc, char **argv)
{
    CLI::App app{"Image Tasker Demo"};

    // defaults for testing in debugger
    int image_cache_size_mb = 16;
    int task_pool_size = 4;
    int request_timeout_ms = 250;
    int width = 612;
    int height = 407;
    std::string image_file_path = std::string(std::filesystem::current_path().string() + "\\..\\tests\\data\\test-image-1.jpg");
    app.add_option("-i", image_file_path, "Full path to a on-disk image to test");
    app.add_option("-x", width, "Width of image to test");
    app.add_option("-y", height, "Height of image to test");
    app.add_option("-c", image_cache_size_mb, "Image cache size in MB");
    app.add_option("-p", task_pool_size, "Number of image tasks in pool");
    app.add_option("-r", request_timeout_ms, "Request timeout for images in ms");

    CLI11_PARSE(app, argc, argv);

    set_level(level::debug);

    info("Starting demo");
    std::shared_ptr<ImageManager> image_manager = std::make_shared<ImageManager>(
        image_cache_size_mb,
        task_pool_size,
        request_timeout_ms);
    std::shared_ptr<TQueue<TaskStatus>> status_queue = std::make_shared<TQueue<TaskStatus>>();
    TaskStatusReporter task_status_reporter(status_queue);
    std::shared_ptr<IImageHandler> image_handler = std::make_shared<TestImageHandler>();

    image_manager->register_task_status_queue(status_queue);
    info("Loading image: " + image_file_path);
    for (int i = 0; i < 100; i++)
    {
        image_manager->request_image_load(image_file_path, image_handler);
    }
    info("Resizing image: " + image_file_path);
    width /= 2;
    height /= 2;
    for (int i = 0; i < 100; i++)
    {
        image_manager->request_image_resize(image_file_path, width, height, image_handler);
        if (i % 25 == 0)
        {
            width /= 2;
            height /= 2;
        }
    }
      
    info("Wait for asyncs to complete");
    std::this_thread::sleep_for(std::chrono::milliseconds(250)); 
    status_queue->stop();

    info("Done");

    return 0;
}
