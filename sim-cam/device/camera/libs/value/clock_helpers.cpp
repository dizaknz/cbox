#include "value/clock_helpers.h"

namespace camera {

uint64_t monotonic_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

int64_t wall_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

camera::v1::Clock make_clock(uint64_t mono_ns, int64_t wall_ms) {
    // FlatBuffers struct: constructed directly, no setters
    return camera::v1::Clock(mono_ns, wall_ms);
}

Timestamps now() { return {monotonic_ns(), wall_ms()}; }

} // namespace camera
