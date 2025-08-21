#include "image_task_manager.h"
#include "unique_queue.hpp"
#include "image_data.h"
#include "task_status.h"

#include <algorithm>
#include <chrono>

ImageTaskRunner::ImageTaskRunner(std::shared_ptr<TQueue<ImageTask>> task_queue)
{
    runner_thread = std::jthread{
        [task_queue, this](std::stop_token ctrl){
            run(ctrl, task_queue);
        }};
}

ImageTaskRunner::~ImageTaskRunner()
{
    stop();
}

void ImageTaskRunner::run(std::stop_token ctrl, std::shared_ptr<TQueue<ImageTask>> task_queue)
{
    while (!ctrl.stop_requested())
    {
        while(task_running.test_and_set(std::memory_order_acquire));
        std::unique_ptr<ImageTask> task = task_queue->dequeue();
        if (task)
        {
            auto start = std::chrono::high_resolution_clock::now();
            if (!task->reader)
            {
                auto errors = std::vector<std::string>{ 
                    "Failed running image task, reason: no image reader provided"
                };
                auto end = std::chrono::high_resolution_clock::now();
                report_task_failure(*task.get(), start - end, errors);
                task_running.clear(std::memory_order_release);
                continue;
            }
            std::vector<std::string> read_errors;
            auto read_result = task->reader->read_image_data(read_errors);
            if (read_errors.size() > 0)
            {
                auto end = std::chrono::high_resolution_clock::now();
                report_task_failure(*task.get(), end - start, read_errors);
                task_running.clear(std::memory_order_release);
                continue;
            }
            if (task->auto_resize && task->resizer)
            {
                start = std::chrono::high_resolution_clock::now();
                std::vector<std::string> resize_errors;
                auto resize_result = task->resizer->resize_image_data(*read_result.get(), resize_errors);
                if (resize_errors.size() > 0)
                {
                    auto end = std::chrono::high_resolution_clock::now();
                    report_task_failure(*task.get(), end - start, resize_errors);
                    task_running.clear(std::memory_order_release);
                    continue;
                }
                if (task->result_queue)
                {
                    auto end = std::chrono::high_resolution_clock::now();
                    task->result_queue->enqueue(std::move(resize_result));
                    report_task_success(*task.get(), end - start);
                }
            }
            if (task->result_queue)
            {
                auto end = std::chrono::high_resolution_clock::now();
                task->result_queue->enqueue(std::move(read_result));
                report_task_success(*task.get(), end - start);
            }
        }
        task_running.clear(std::memory_order_release);
    }
}

void ImageTaskRunner::stop()
{
    // spin until current task is done
    while(!task_running.test());
    runner_thread.request_stop();
}

void ImageTaskRunner::report_task_failure(const ImageTask& task, std::chrono::duration<double> duration, std::vector<std::string> errors)
{
    if (!task.status_queue)
    {
        return;
    }
    task.status_queue->enqueue(std::make_unique<TaskStatus>(
        task.task_id,
        TaskState::Failed,
        duration,
        errors));
}

void ImageTaskRunner::report_task_success(const ImageTask& task, std::chrono::duration<double> duration)
{
    if (!task.status_queue)
    {
        return;
    }
    task.status_queue->enqueue(std::make_unique<TaskStatus>(
        task.task_id,
        TaskState::Completed,
        duration));
}

ImageTaskManager::ImageTaskManager() : ImageTaskManager(DEFAULT_POOL_SIZE)
{}

ImageTaskManager::ImageTaskManager(unsigned int pool_size)
{
    task_queue = std::make_shared<TQueue<ImageTask>>();
    if (pool_size < 1)
    {
        this->pool_size = DEFAULT_POOL_SIZE;
    }
    else
    {
        this->pool_size = std::min(pool_size, std::thread::hardware_concurrency());
    }
    while (runner_pool.size() != this->pool_size)
    {
        runner_pool.emplace_back(std::make_unique<ImageTaskRunner>(task_queue));
    }
}

ImageTaskManager::~ImageTaskManager()
{
    // workaround for sharing queue
    task_queue->stop();

    // lock and request tasks in pool to stop by clearing pool
    const std::lock_guard<std::mutex> lock(pool_mutex);
    runner_pool.clear();
}

void ImageTaskManager::increase_pool_size_by(unsigned int delta_pool_size)
{
    if (delta_pool_size < 1)
    {
        // no-op, shrinking pool not currently supported
        return;
    }
    // lock the pool and resize
    const std::lock_guard<std::mutex> lock(pool_mutex);
    pool_size += delta_pool_size;
    while (runner_pool.size() != pool_size)
    {
        runner_pool.emplace_back(std::make_unique<ImageTaskRunner>(task_queue));
    }
}

void ImageTaskManager::submit_task(ImageTask *const task)
{
    task_queue->enqueue(std::unique_ptr<ImageTask>(task));
}
