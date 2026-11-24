#pragma once
// reactor.h — Single-threaded event reactor (epoll / IOCP)
// All I/O is non-blocking; handlers never block.
// Work that would block runs on a bounded worker thread pool.

#include "value/clock_helpers.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

namespace camera {

// ── Event types for addFd ────────────────────────────────────────

enum class EventFlags : uint16_t {
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1,
    Error = 1 << 2,
};

inline EventFlags operator|(EventFlags a, EventFlags b) {
    return static_cast<EventFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
inline EventFlags operator&(EventFlags a, EventFlags b) {
    return static_cast<EventFlags>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

// ── Timer handle ─────────────────────────────────────────────────

using TimerId = uint32_t;

// ── Task (posted work item) ──────────────────────────────────────

using Task = std::function<void()>;

// ── Reactor ──────────────────────────────────────────────────────

class Reactor {
  public:
    Reactor();
    ~Reactor();

    // Non-copyable
    Reactor(const Reactor &) = delete;
    Reactor &operator=(const Reactor &) = delete;

    // Add a file descriptor handler. Returns the fd for removal.
    // event_handler is called when any of the requested events occur.
    int addFd(int fd, EventFlags events, std::function<void(EventFlags)> event_handler);

    // Remove a previously added fd handler.
    void removeFd(int fd);

    // Add a one-shot or recurring timer. Returns a TimerId.
    // interval_ms: 0 = one-shot; >0 = recurring
    TimerId addTimer(uint64_t interval_ms, std::function<void()> callback);

    // Cancel a timer by ID.
    void cancelTimer(TimerId id);

    // Post a task to be executed on the reactor thread (thread-safe).
    void post(Task task);

    // Run the event loop (blocks until stop() is called or no fds/timers/tasks remain).
    void run();

    // Signal the reactor to stop after the current iteration.
    void stop();

    // Ping the watchdog (call periodically from the reactor thread).
    void watchdogPing();

    // ── Worker thread pool for blocking work ──────────────────────

    // Submit a blocking task; result is returned via a callback on the reactor thread.
    template <typename F, typename R> void spawn(F &&f, std::function<void(R)> on_complete);

  private:
    struct FdEntry {
        int fd;
        EventFlags events;
        std::function<void(EventFlags)> handler;
    };

    struct TimerEntry {
        TimerId id;
        uint64_t due_ns;      // absolute monotonic ns
        uint64_t interval_ms; // 0 = one-shot
        std::function<void()> callback;
    };

    void dispatchTimers(uint64_t now_ns);
    void drainTasks();

    // Internal pipe for self-notification (post, stop, etc.)
    int wake_pipe_[2] = {-1, -1};

    // Linux: epoll file descriptor (unused on Windows)
    int epoll_fd_ = -1;

    // Storage
    std::vector<FdEntry> fds_;
    std::vector<TimerEntry> timers_;
    TimerId next_timer_id_ = 1;

    // Task queue (protected by mutex for thread-safe post())
    std::mutex task_mutex_;
    std::queue<Task> tasks_;
    std::condition_variable task_cv_;

    // Running flag
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};

    // Worker pool
    std::vector<std::thread> workers_;
    std::queue<std::pair<std::function<void()>, std::function<void()>>> work_queue_;
    std::condition_variable work_cv_;
    std::atomic<bool> workers_running_{false};

    void workerLoop();
};

// ── Inline template implementation ───────────────────────────────

template <typename F, typename R> void Reactor::spawn(F &&f, std::function<void(R)> on_complete) {
    // Wrap: call f(), then post result callback to reactor thread
    auto wrapper = [f = std::forward<F>(f), on_complete = std::move(on_complete)]() mutable {
        try {
            R result = f();
            if (on_complete)
                on_complete(std::move(result));
        } catch (...) {
            // Catch-all: don't let exceptions escape worker threads
            if (on_complete)
                on_complete();
        }
    };

    std::lock_guard<std::mutex> lock(task_mutex_);
    work_queue_.push({std::move(wrapper), nullptr});
    work_cv_.notify_one();
}

} // namespace camera
