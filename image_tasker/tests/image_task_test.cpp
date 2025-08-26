#include <iostream>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <filesystem>

#include "image_data.h"
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
            std::optional<TaskStatus> task_status = status_queue->dequeue();
            if (task_status.has_value())
            {
                auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(task_status->duration);
                std::cout << "[Task Status - " 
                    << "task: " << task_status->task_id
                    << " state: " << task_state_to_string(task_status->state)
                    << (task_status->state == TaskState::Failed ? " error: " + task_status->errors[0] : "")
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

TEST(ImageTask, Load)
{
    std::unique_ptr<ImageTaskManager> task_manager = std::make_unique<ImageTaskManager>();
    std::shared_ptr<TQueue<TaskStatus>> status_queue = std::make_shared<TQueue<TaskStatus>>();
    std::shared_ptr<TQueue<std::unique_ptr<ImageData>>> image_queue = std::make_shared<TQueue<std::unique_ptr<ImageData>>>();
    TaskStatusReporter task_status_reporter(status_queue);

    for (int i = 0; i < 10; i++)
    {
        std::string task_id = "image loader: " + std::to_string(i+1);
        std::string file = std::string(std::filesystem::current_path().string() + "\\..\\tests\\data\\test-image-1.jpg");
        task_manager->submit_task(ImageTask(
            task_id,
            file,
            status_queue,
            image_queue
        ));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50 * 20)); 
    status_queue->shutdown();
}

TEST(ImageTask, Resize)
{
    std::unique_ptr<ImageTaskManager> task_manager = std::make_unique<ImageTaskManager>();
    std::shared_ptr<TQueue<TaskStatus>> status_queue = std::make_shared<TQueue<TaskStatus>>();
    std::shared_ptr<TQueue<std::unique_ptr<ImageData>>> image_queue = std::make_shared<TQueue<std::unique_ptr<ImageData>>>();
    TaskStatusReporter task_status_reporter(status_queue);

    for (int i = 0; i < 10; i++)
    {
        std::string file = std::string(std::filesystem::current_path().string() + "\\..\\tests\\data\\test-image-1.jpg");
        task_manager->submit_task(ImageTask(
            "image resizer: " + std::to_string(i+1),
            file,
            64, 
            64,
            status_queue,
            image_queue
        ));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50 * 20)); 
    status_queue->shutdown();
}
