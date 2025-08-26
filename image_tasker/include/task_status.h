#pragma once

#include <string>
#include <unordered_map>

enum class TaskState: unsigned char
{
    NotRunning = 0,
    Starting,
    Running,
    Completed,
    Failed
};

static std::string task_state_to_string(const TaskState state)
{
    static std::unordered_map<TaskState, std::string> state_lookup =
    {
        { TaskState::NotRunning, "Not Running" },
        { TaskState::Starting,   "Starting"    },
        { TaskState::Running,    "Running"     },
        { TaskState::Completed,  "Completed"   },
        { TaskState::Failed,     "Failed"      }
    };
    return state_lookup.count(state)
        ? state_lookup[state]
        : "Unknown";
}

/**
 * Define the status properties of a task
 */
struct TaskStatus
{
    std::string task_id;
    TaskState state;
    std::chrono::duration<double> duration;
    std::vector<std::string> errors;

    TaskStatus() = delete;
    TaskStatus(const TaskStatus&) = delete;
    TaskStatus(
        const std::string& task_id,
        const TaskState state,
        std::chrono::duration<double> duration,
        const std::vector<std::string>& errors)
    : task_id(task_id), state(state), duration(duration), errors(errors)
    {}
    TaskStatus(
        const std::string& task_id,
        const TaskState state,
        std::chrono::duration<double> duration)
    : TaskStatus(task_id, state, duration, {})
    {}
    TaskStatus(TaskStatus &&other) noexcept
    : task_id(std::move(other.task_id)),
      state(other.state),
      duration(other.duration),
      errors(std::move(other.errors))
    {}
    void operator=(const TaskStatus&) = delete;
};
