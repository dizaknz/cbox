#pragma once
// uuid.h — UUIDv7-like identifier generation

#include "generated/common_generated.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <random>

#ifdef _WIN32
#include <windows.h>
#endif

namespace camera {

struct UuidV7 {
    uint64_t hi; // top 48 bits: millisecond timestamp | top 16 bits: random
    uint64_t lo; // bottom 64 bits: random
};

// Generate a UUIDv7-layout ID (monotonic-ish, unique)
UuidV7 generate_uuid_v7();

// Convert to FlatBuffers Uuid
camera::v1::Uuid to_fb_uuid(UuidV7 id);

// Parse from byte array (16 bytes, big-endian)
std::optional<UuidV7> parse_uuid_v7(const uint8_t bytes[16]);

} // namespace camera
