#pragma once

#include "ota_port.hpp"
#include "memory_pool.hpp"

#include <coroutine>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <functional>

/**
 * @brief Callback types for OTA progress events.
 */
struct OTAFirmwareInfo {
    std::size_t size;
    std::string_view version;
    std::string_view checksum;
};

using ProgressCallback  = std::function<void(std::size_t bytes_written,
                                             std::size_t total)>;
using ErrorCallback     = std::function<void(const std::string& error)>;
using StatusCallback    = std::function<void(std::string_view status)>;

/**
 * @brief Configuration for the OTA update process.
 */
struct OTAConfig {
    std::size_t chunk_size   = 256;     // Bytes per read/write operation
    std::size_t flash_page   = 4096;    // Flash page size to erase before writing
    bool         verify_after_write = true;  // Verify after each page
    ProgressCallback on_progress;
    ErrorCallback    on_error;
    StatusCallback   on_status;
};

/**
 * @brief Coroutine-based OTA update handler.
 *
 * Drives the firmware download → flash write → verify lifecycle using C++20
 * coroutines. All allocations are routed through a MemoryPool for embedded
 * systems with no heap.
 */
class OTATask {
public:
    struct promise_type {
        OTATask get_return_object() {
            return OTATask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept  { return {}; }
        std::suspend_always final_suspend() noexcept    { return {}; }
        void return_void()                              {}
        void unhandled_exception()                      { std::terminate(); }

        // Allocate coroutine frame from the external MemoryPool
        void* operator new(std::size_t size) {
            auto& pool = OTATask::get_pool_ref();
            std::cout << "  [MemPool] Allocating coroutine frame: "
                      << size << " bytes (remaining: " << pool.remaining() << ")\n";
            return pool.get_resource().allocate(size);
        }

        void operator delete(void* ptr, std::size_t size) {
            auto& pool = OTATask::get_pool_ref();
            pool.get_resource().deallocate(ptr, size);
        }
    };

    using Handle = std::coroutine_handle<promise_type>;

    explicit OTATask(Handle h) : handle_(h) {}
    ~OTATask() { if (handle_) handle_.destroy(); }

    // Non-copyable, movable
    OTATask(OTATask&& other) noexcept : handle_(other.handle_) { other.handle_ = {}; }
    OTATask& operator=(OTATask&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = {};
        }
        return *this;
    }
    OTATask(const OTATask&)            = delete;
    OTATask& operator=(const OTATask&) = delete;

    /**
     * @brief Resume the coroutine one step. Returns false when done.
     */
    bool resume() {
        if (!handle_ || handle_.done()) return false;
        handle_.resume();
        return !handle_.done();
    }

    bool is_done() const { return !handle_ || handle_.done(); }

    /**
     * @brief Set the MemoryPool for coroutine frame allocation.
     */
    static void set_pool(MemoryPool& pool) { pool_ptr_ = &pool; }

private:
    Handle handle_;

    // Static pointer to the memory pool (set once at startup)
    static MemoryPool* pool_ptr_;

    static MemoryPool& get_pool_ref() {
        if (!pool_ptr_) throw std::runtime_error("MemoryPool not set for OTATask");
        return *pool_ptr_;
    }
};

// ── OTAHandler: high-level orchestrator ──────────────────────────────────────

/**
 * @brief High-level OTA handler that owns the port and drives the update.
 *
 * Usage:
 *   MemoryPool pool;
 *   SimulatedOTAPort port;
 *   OTAHandler handler(port, config);
 *   bool success = handler.run();
 */
class OTAHandler {
public:
    explicit OTAHandler(OTAPort& port, const OTAConfig& config = {})
        : port_(port), config_(config) {}

    /**
     * @brief Run the full OTA update sequence.
     * @return true if the update completed successfully.
     */
    bool run();

private:
    OTAPort&        port_;
    const OTAConfig config_;

    // Coroutine that performs the actual firmware download + flash write
    struct UpdateCoro {
        struct promise_type : OTATask::promise_type {};
        using handle_t = std::coroutine_handle<promise_type>;

        // Progress tracking
        std::size_t total_bytes   = 0;
        std::size_t bytes_written = 0;
        bool        success       = false;
        std::string error_message;

        // References to external state
        OTAPort& port;
        OTAConfig config;
        ProgressCallback on_progress;
    };

    UpdateCoro::handle_t create_update_coroutine();
};
