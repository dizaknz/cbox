#include "clock.h"
#include <algorithm>
#include <vector>

namespace camera {

SimClock SimClock::realtime() { return SimClock(); }

SimClock SimClock::create(int64_t start_ms) { return SimClock(start_ms); }

uint64_t SimClock::monotonic_ns() const {
    if (!simulated_) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
    // Return simulated monotonic time: offset + elapsed since clock creation
    uint64_t real_now =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    return monotonic_offset_ns_ + (real_now - monotonic_offset_ns_);
}

int64_t SimClock::wall_ms() const {
    if (!simulated_) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch()
        )
            .count();
    }
    return wall_ms_;
}

void SimClock::advance(int64_t delta_ms) {
    if (!simulated_)
        return;

    int64_t old_wall = wall_ms_;
    wall_ms_ += delta_ms;

    // Check wall callbacks
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    auto it = wall_callbacks_.begin();
    while (it != wall_callbacks_.end()) {
        if (old_wall < it->first && wall_ms_ >= it->first) {
            it->second(it->first);
            it = wall_callbacks_.erase(it);
        } else if (wall_ms_ < it->first) {
            // Wall time hasn't reached this callback yet, stop scanning
            break;
        } else {
            ++it;
        }
    }
}

void SimClock::set_time(int64_t new_time_ms) {
    if (!simulated_)
        return;
    int64_t old_wall = wall_ms_;
    wall_ms_ = new_time_ms;

    // Check wall callbacks for time that was jumped over
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    auto it = wall_callbacks_.begin();
    while (it != wall_callbacks_.end()) {
        if (old_wall < it->first && wall_ms_ >= it->first) {
            it->second(it->first);
            it = wall_callbacks_.erase(it);
        } else {
            ++it;
        }
    }
}

void SimClock::onWallMs(int64_t target_ms, WallCallback cb) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    wall_callbacks_.push_back({target_ms, std::move(cb)});
    // Sort by target time for efficient checking
    std::sort(wall_callbacks_.begin(), wall_callbacks_.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });
}

} // namespace camera
