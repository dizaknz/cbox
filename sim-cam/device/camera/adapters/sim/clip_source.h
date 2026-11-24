#pragma once
// clip_source.h — SyntheticClipSource: generates deterministic media byte streams
// Replaces the real camera sensor's video stream with synthetic data.

#include "value/result.h"
#include <cstdint>
#include <string>
#include <vector>

namespace camera {

// ── SyntheticClipSource ──────────────────────────────────────────
// Generates a deterministic byte stream that mimics an H.264 clip:
// - 32-byte header (magic, version, duration_ms, size_bytes, sha256)
// - Payload data (deterministic pseudo-random based on clip_id)
// - 32-byte trailer (checksum = SHA-256 of header + payload)

struct ClipHeader {
    uint8_t magic[4];     // "SYNT"
    uint16_t version;     // always 1
    uint32_t duration_ms; // clip duration
    uint64_t size_bytes;  // total size (header + payload + trailer)
    uint8_t sha256[32];   // hash of header + payload (filled after generation)
};

struct ClipTrailer {
    uint8_t checksum[32]; // SHA-256(header + payload)
};

class SyntheticClipSource {
  public:
    static constexpr size_t HEADER_SIZE = 32;
    static constexpr size_t TRAILER_SIZE = 32;
    static constexpr size_t MIN_PAYLOAD_SIZE = 4096; // Minimum clip payload size

    // Generate a synthetic clip. Returns the complete byte stream (header + payload + trailer).
    // duration_ms: desired clip length
    // max_bytes: total maximum size including header and trailer
    ResultT<std::vector<uint8_t>> generate(uint32_t duration_ms, uint64_t max_bytes);

    // Validate a clip's checksum. Returns true if valid.
    static bool validate(const std::vector<uint8_t> &clip);

  private:
    // Simple deterministic hash (not cryptographically secure — sim only)
    static void compute_checksum(const uint8_t *data, size_t len, uint8_t out[32]);

    // Generate pseudo-random payload bytes deterministically from a seed
    static void generate_payload(uint8_t *buf, size_t len, uint64_t seed);
};

} // namespace camera
