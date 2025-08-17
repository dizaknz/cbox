#include <iostream>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <filesystem>

#include "image_manager.h"
#include "task_status.h"
#include "unique_queue.hpp"
#include "image_task_manager.h"

#include <gtest/gtest.h>

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
                std::cout << "[Task Status - " 
                    << "task: " << task_status->task_id
                    << " state: " << task_state_to_string(task_status->state)
                    << " time elapsed: " << elapsed_time.count()
                    << "]" << std::endl;
                EXPECT_EQ(task_status->state, TaskState::Completed);
                EXPECT_EQ(task_status->errors.size(), 0);
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
        std::cout << "Image: " << image_data.source_file_path
            << " Width: " << std::to_string(image_data.width)
            << " Height: " << std::to_string(image_data.height)
            << std::endl;
    }
};

TEST(ImageManager, Load)
{
    const int image_cache_size_mb = 16;
    const int task_pool_size = 4;
    const int request_timeout_ms = 25;

    std::shared_ptr<ImageManager> image_manager = std::make_shared<ImageManager>(
        image_cache_size_mb,
        task_pool_size,
        request_timeout_ms);
    std::shared_ptr<TQueue<TaskStatus>> status_queue = std::make_shared<TQueue<TaskStatus>>();
    TaskStatusReporter task_status_reporter(status_queue);
    std::shared_ptr<IImageHandler> image_handler = std::make_shared<TestImageHandler>();

    std::string file = std::string(std::filesystem::current_path().string() + "\\..\\tests\\data\\test-image-1.jpg");

    image_manager->register_task_status_queue(status_queue);
    for (int i = 0; i < 10; i++)
    {
        image_manager->async_request_image_load(file, 612, 407, image_handler);
    }
     
    std::this_thread::sleep_for(std::chrono::milliseconds(250)); 
    status_queue->stop();
}

TEST(ImageManager, Resize)
{
    const int image_cache_size_mb = 16;
    const int task_pool_size = 4;
    const int request_timeout_ms = 25;

    std::shared_ptr<ImageManager> image_manager = std::make_shared<ImageManager>(
        image_cache_size_mb,
        task_pool_size,
        request_timeout_ms);
    std::shared_ptr<TQueue<TaskStatus>> status_queue = std::make_shared<TQueue<TaskStatus>>();
    TaskStatusReporter task_status_reporter(status_queue);
    std::shared_ptr<IImageHandler> image_handler = std::make_shared<TestImageHandler>();

    std::string file = std::string(std::filesystem::current_path().string() + "\\..\\tests\\data\\test-image-1.jpg");

    image_manager->register_task_status_queue(status_queue);
    for (int i = 0; i < 10; i++)
    {
        image_manager->async_request_image_resize(file, 61, 40, image_handler);
    }
     
    std::this_thread::sleep_for(std::chrono::milliseconds(250)); 
    status_queue->stop();
}
