#include "reactor/reactor.h"

#ifdef _WIN32
// clang-format off
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
// clang-format on
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#endif

namespace camera {

// ── Cross-platform wake pipe helpers ─────────────────────────────

#ifdef _WIN32
static int win_pipe(int sv[2]) {
    // Windows: use socketpair-like approach with anonymous pipes via CreatePipe
    HANDLE rh, wh;
    if (!CreatePipe(&rh, &wh, nullptr, 0))
        return -1;
    // Set read handle to non-inheritable and non-blocking
    DWORD mode = PIPE_NOWAIT;
    SetNamedPipeHandleState(wh, &mode, nullptr, nullptr);
    sv[0] = _open_osfhandle(reinterpret_cast<intptr_t>(rh), 0);
    sv[1] = _open_osfhandle(reinterpret_cast<intptr_t>(wh), 0);
    return 0;
}

static int win_write(int fd, const void *buf, size_t len) {
    HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    DWORD written;
    return WriteFile(h, buf, static_cast<DWORD>(len), &written, nullptr) ? static_cast<int>(written) : -1;
}

static int win_read(int fd, void *buf, size_t len) {
    HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    DWORD read;
    return ReadFile(h, buf, static_cast<DWORD>(len), &read, nullptr) ? static_cast<int>(read) : -1;
}

static void win_close(int fd) {
    if (fd >= 0)
        _close(fd);
}
#else
#define win_pipe(a) pipe2((a), O_NONBLOCK | O_CLOEXEC)
#define win_write(fd, buf, len) write((fd), (buf), (len))
#define win_read(fd, buf, len) read((fd), (buf), (len))
#define win_close(fd) close(fd)
#endif

// ── Reactor construction / destruction ───────────────────────────

Reactor::Reactor() {
    // Create self-wake pipe
    if (win_pipe(wake_pipe_) < 0) {
        wake_pipe_[0] = -1;
        wake_pipe_[1] = -1;
    }

    // Start worker threads (bounded pool)
    workers_running_ = true;
    for (int i = 0; i < 4; ++i) {
        workers_.emplace_back(&Reactor::workerLoop, this);
    }
}

Reactor::~Reactor() {
    workers_running_ = false;
    work_cv_.notify_all();
    for (auto &w : workers_) {
        if (w.joinable())
            w.join();
    }

    win_close(wake_pipe_[0]);
    win_close(wake_pipe_[1]);
}

// ── Linux/Unix: epoll-based reactor ─────────────────────────────

#ifndef _WIN32

int Reactor::addFd(int fd, EventFlags events, std::function<void(EventFlags)> event_handler) {
    FdEntry entry{fd, events, std::move(event_handler)};
    fds_.push_back(std::move(entry));

    struct epoll_event ev{};
    ev.events = static_cast<uint32_t>(events);
    ev.data.fd = fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);

    return fd;
}

void Reactor::removeFd(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    fds_.erase(std::remove_if(fds_.begin(), fds_.end(), [fd](const FdEntry &e) { return e.fd == fd; }), fds_.end());
}

TimerId Reactor::addTimer(uint64_t interval_ms, std::function<void()> callback) {
    TimerId id = next_timer_id_++;
    auto now = monotonic_ns();
    uint64_t due = (interval_ms == 0) ? 0 : now + interval_ms * 1000000ULL;
    timers_.push_back({id, due, interval_ms, std::move(callback)});
    return id;
}

void Reactor::cancelTimer(TimerId id) {
    timers_.erase(
        std::remove_if(timers_.begin(), timers_.end(), [id](const TimerEntry &t) { return t.id == id; }), timers_.end()
    );
}

void Reactor::post(Task task) {
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        tasks_.push(std::move(task));
    }
    uint64_t val = 1;
    win_write(wake_pipe_[1], &val, sizeof(val));
}

void Reactor::stop() {
    stopped_ = true;
    uint64_t val = 1;
    win_write(wake_pipe_[1], &val, sizeof(val));
}

void Reactor::watchdogPing() {}

void Reactor::run() {
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0)
        return;

    running_ = true;
    stopped_ = false;

    // Add wake pipe to epoll
    struct epoll_event wake_ev{};
    wake_ev.events = EPOLLIN;
    wake_ev.data.fd = wake_pipe_[0];
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, wake_pipe_[0], &wake_ev);

    // Add all fds to epoll
    for (auto &fd_entry : fds_) {
        struct epoll_event ev{};
        ev.events = static_cast<uint32_t>(fd_entry.events);
        ev.data.fd = fd_entry.fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_entry.fd, &ev);
    }

    constexpr int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];

    while (!stopped_) {
        drainTasks();

        // Calculate timeout for epoll_wait based on nearest timer
        int timeout_ms = -1;
        uint64_t now_ns = monotonic_ns();
        for (auto &t : timers_) {
            if (t.due_ns > 0) {
                int64_t remaining_ms = static_cast<int64_t>((t.due_ns - now_ns) / 1000000ULL);
                if (remaining_ms <= 0) {
                    dispatchTimers(now_ns);
                    timeout_ms = 0;
                    break;
                }
                if (timeout_ms < 0 || remaining_ms < timeout_ms) {
                    timeout_ms = static_cast<int>(remaining_ms);
                }
            }
        }

        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout_ms);
        now_ns = monotonic_ns();

        if (n > 0) {
            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;

                if (fd == wake_pipe_[0]) {
                    uint64_t val;
                    win_read(fd, &val, sizeof(val));
                    drainTasks();
                    continue;
                }

                for (auto &fd_entry : fds_) {
                    if (fd_entry.fd == fd) {
                        EventFlags triggered = static_cast<EventFlags>(events[i].events);
                        fd_entry.handler(triggered);
                        break;
                    }
                }
            }
        }

        dispatchTimers(now_ns);
    }

    win_close(epoll_fd);
    running_ = false;
}

#else

// ── Windows stub: minimal select-based reactor ──────────────────

int Reactor::addFd(int fd, EventFlags events, std::function<void(EventFlags)> event_handler) {
    (void)events;
    FdEntry entry{fd, events, std::move(event_handler)};
    fds_.push_back(std::move(entry));
    return fd;
}

void Reactor::removeFd(int fd) {
    fds_.erase(std::remove_if(fds_.begin(), fds_.end(), [fd](const FdEntry &e) { return e.fd == fd; }), fds_.end());
}

TimerId Reactor::addTimer(uint64_t interval_ms, std::function<void()> callback) {
    TimerId id = next_timer_id_++;
    uint64_t now = monotonic_ns();
    uint64_t due = (interval_ms == 0) ? 0 : now + interval_ms * 1000000ULL;
    timers_.push_back({id, due, interval_ms, std::move(callback)});
    return id;
}

void Reactor::cancelTimer(TimerId id) {
    timers_.erase(
        std::remove_if(timers_.begin(), timers_.end(), [id](const TimerEntry &t) { return t.id == id; }), timers_.end()
    );
}

void Reactor::post(Task task) {
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        tasks_.push(std::move(task));
    }
    uint64_t val = 1;
    win_write(wake_pipe_[1], &val, sizeof(val));
}

void Reactor::stop() {
    stopped_ = true;
    uint64_t val = 1;
    win_write(wake_pipe_[1], &val, sizeof(val));
}

void Reactor::watchdogPing() {}

void Reactor::run() {
    running_ = true;
    stopped_ = false;

    while (!stopped_) {
        drainTasks();
        dispatchTimers(monotonic_ns());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    running_ = false;
}

#endif // _WIN32

// ── Common: timer dispatch & task draining ───────────────────────

void Reactor::dispatchTimers(uint64_t now_ns) {
    for (auto it = timers_.begin(); it != timers_.end();) {
        if (it->due_ns > 0 && now_ns >= it->due_ns) {
            it->callback();
            if (it->interval_ms == 0) {
                it = timers_.erase(it);
            } else {
                it->due_ns = now_ns + it->interval_ms * 1000000ULL;
                ++it;
            }
        } else {
            ++it;
        }
    }
}

void Reactor::drainTasks() {
    std::queue<Task> local;
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        tasks_.swap(local);
    }
    while (!local.empty()) {
        auto task = std::move(local.front());
        local.pop();
        task();
    }
}

// ── Worker thread loop (common) ──────────────────────────────────

void Reactor::workerLoop() {
    while (workers_running_) {
        std::function<void()> work;
        {
            std::unique_lock<std::mutex> lock(task_mutex_);
            work_cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !work_queue_.empty() || !workers_running_;
            });
            if (!workers_running_)
                return;
            if (work_queue_.empty())
                continue;
            work = std::move(work_queue_.front().first);
            work_queue_.pop();
        }
        if (work)
            work();
    }
}

} // namespace camera
