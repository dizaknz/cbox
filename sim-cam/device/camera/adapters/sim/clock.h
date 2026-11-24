#pragma once
// clock.h — SimClock: injectable clock for deterministic simulation timing
// Replaces steady_clock and system_clock with a controllable time source.

#include "reactor/reactor.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>

namespace camera {

// ── SimClock ─────────────────────────────────────────────────────
// Provides a deterministic clock that can be advanced programmatically.
// When in "realtime" mode, it returns the actual system time.

class SimClock {
  public:
    // Create a SimClock in realtime mode (returns actual system time)
    static SimClock realtime();

    // Create a SimClock at a specific simulated time (milliseconds since epoch)
    static SimClock create(int64_t start_ms);

    // Get current monotonic nanoseconds
    uint64_t monotonic_ns() const;

    // Get current wall milliseconds
    int64_t wall_ms() const;

    // Advance the clock by delta_ms (only effective in simulated mode)
    void advance(int64_t delta_ms);

    // Set the absolute time to new_time_ms (only effective in simulated mode)
    void set_time(int64_t new_time_ms);

    // Check if in simulated mode
    bool is_simulated() const { return simulated_; }

    // Register a callback to be invoked when the clock advances past a wall_ms threshold.
    using WallCallback = std::function<void(int64_t wall_ms)>;
    void onWallMs(int64_t target_ms, WallCallback cb);

  private:
    SimClock() : simulated_(false), wall_ms_(0), monotonic_offset_ns_(0) {}
    SimClock(int64_t start_ms) : simulated_(true), wall_ms_(start_ms), monotonic_offset_ns_(monotonic_ns()) {}

    bool simulated_;
    int64_t wall_ms_;
    uint64_t monotonic_offset_ns_;

    std::mutex callbacks_mutex_;
    std::vector<std::pair<int64_t, WallCallback>> wall_callbacks_;
};

} // namespace camera
