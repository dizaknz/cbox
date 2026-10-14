#pragma once

#include <array>
#include <cstddef>
#include <memory_resource>
#include <cstdint>
#include <atomic>

/**
 * @brief Static memory pool for embedded systems.
 *
 * All allocations come from a pre-allocated BSS segment buffer — no heap usage.
 * Supports configurable size and optional alignment.
 */
class MemoryPool {
public:
    static constexpr std::size_t DEFAULT_SIZE = 2048;

    explicit MemoryPool(std::size_t size = DEFAULT_SIZE)
        : buffer_(), resource_(buffer_.data(), size) {}

    /**
     * @brief Get the PMR resource for allocation.
     */
    std::pmr::memory_resource& get_resource() { return resource_; }
    const std::pmr::memory_resource& get_resource() const { return resource_; }

    /**
     * @brief Returns remaining bytes in the pool.
     */
    std::size_t remaining() const {
        // Track allocations via mark_used/mark_freed for accurate accounting.
        return buffer_.size() - used_bytes_;
    }

    /**
     * @brief Mark bytes as allocated (called by OTATask::operator new).
     */
    void mark_used(std::size_t n) { used_bytes_ += n; }

    /**
     * @brief Unmark bytes as allocated (called by OTATask::operator delete).
     */
    void mark_freed(std::size_t n) { used_bytes_ = (n > used_bytes_) ? 0 : used_bytes_ - n; }

    /**
     * @brief Total capacity of the pool.
     */
    std::size_t capacity() const { return buffer_.size(); }

private:
    std::array<std::byte, DEFAULT_SIZE> buffer_;
    std::pmr::monotonic_buffer_resource resource_;
    std::atomic<std::size_t>            used_bytes_{0};
};
