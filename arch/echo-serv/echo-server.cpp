#include <coroutine>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

//
// Coroutine Return Object (Task Framework)
//
struct [[nodiscard]] Task {
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { 
            return {};
        }

        std::suspend_always final_suspend() noexcept {
            return {};
        }

        void return_void() {}
        void unhandled_exception() {
            std::terminate();
        }
    };
    std::coroutine_handle<promise_type> handle;
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() { 
        if (handle) {
            handle.destroy();
        }
    }
};

//
// Reactor Event Loop
//
class EpollLoop {
  public:
    EpollLoop() {
        epoll_fd = epoll_create1(0);
        if (epoll_fd < 0)
            throw std::runtime_error("Failed to create epoll instance");
    }

    ~EpollLoop() { close(epoll_fd); }

    void register_read(int fd, std::coroutine_handle<> handle) {
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLONESHOT;
        ev.data.ptr = handle.address();

        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
                perror("epoll_ctl failed");
            }
        }
    }

    void run_once() {
        constexpr int MAX_EVENTS = 64;
        epoll_event events[MAX_EVENTS];

        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < nfds; ++i) {
            // Retrieve the suspended coroutine handle from the epoll event context
            auto handle = std::coroutine_handle<>::from_address(events[i].data.ptr);
            if (handle && !handle.done()) {
                handle.resume();
            }
        }
    }

  private:
    int epoll_fd;
};

// Global loop context
EpollLoop g_loop;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

//
// Bridge: Awaiter Object for Network I/O
//
struct SocketAwaiter {
    int fd;

    bool await_ready() const noexcept { 
        return false;
    }

    // If the read would block, this method suspends the coroutine
    // and passes the handle directly into our Epoll event loop.
    void await_suspend(std::coroutine_handle<> handle) const noexcept { 
        g_loop.register_read(fd, handle);
    }

    void await_resume() const noexcept {}
};

//
// Event-Driven Coroutines
//
Task handle_client(int client_fd) {
    char buffer[1024];

    while (true) {
        // Suspend execution until epoll flags this socket descriptor as readable
        co_await SocketAwaiter{client_fd};

        std::memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received > 0) {
            std::cout << "[Client " << client_fd << "] " << buffer;
            send(client_fd, buffer, bytes_received, 0);
        } else {
            std::cout << "[Client " << client_fd << "] Disconnected.\n";
            close(client_fd);
            co_return; 
        }
    }
}

Task accept_connections(int server_fd) {
    while (true) {
        // Suspend until a new client tries to connect
        co_await SocketAwaiter{server_fd};

        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd >= 0) {
            std::cout << "[Server] Accepted new connection on FD: " << client_fd << "\n";
            set_nonblocking(client_fd);

            // Spawn a distinct lifecycle coroutine for this specific client
            auto client_task = handle_client(client_fd);
            client_task.handle.resume(); 
            client_task.handle = nullptr;
        }
    }
}

int main() {
    constexpr int PORT = 8080;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(server_fd);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return 1;
    }
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        return 1;
    }

    std::cout << "[Server] Listening on port " << PORT << "\n";

    auto listener = accept_connections(server_fd);
    listener.handle.resume();
    // Hand off ownership to the loop runtime mechanics
    listener.handle = nullptr;

    while (true) {
        g_loop.run_once();
    }

    return 0;
}
