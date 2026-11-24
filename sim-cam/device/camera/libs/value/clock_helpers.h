#pragma once
// clock_helpers.h — monotonic and wall time utilities

#include "generated/common_generated.h"
#include <chrono>
#include <cstdint>

namespace camera {

struct Timestamps {
    uint64_t mono_ns; // from CLOCK_MONOTONIC
    int64_t wall_ms;  // from CLOCK_REALTIME (may be wrong)
};

// Get current monotonic nanoseconds
uint64_t monotonic_ns();

// Get current wall time in milliseconds
int64_t wall_ms();

// Build a Clock FlatBuffer struct from both sources
camera::v1::Clock make_clock(uint64_t mono_ns, int64_t wall_ms);

// Get both at once (single call for consistency)
Timestamps now();

} // namespace camera
