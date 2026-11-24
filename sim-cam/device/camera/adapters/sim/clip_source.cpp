#include "clip_source.h"
#include <cstring>

namespace camera {

ResultT<std::vector<uint8_t>> SyntheticClipSource::generate(uint32_t duration_ms, uint64_t max_bytes) {
    if (max_bytes < HEADER_SIZE + TRAILER_SIZE + MIN_PAYLOAD_SIZE) {
        return ResultT<std::vector<uint8_t>>::err(make_error(Error::Code::InvalidArgument, "max_bytes too small"));
    }

    size_t payload_size = static_cast<size_t>(max_bytes - HEADER_SIZE - TRAILER_SIZE);
    std::vector<uint8_t> clip(max_bytes);

    // Write header
    ClipHeader *header = reinterpret_cast<ClipHeader *>(clip.data());
    std::memcpy(header->magic, "SYNT", 4);
    header->version = 1;
    header->duration_ms = duration_ms;
    header->size_bytes = max_bytes;

    // Generate deterministic payload (seeded from duration for reproducibility)
    generate_payload(clip.data() + HEADER_SIZE, payload_size, duration_ms);

    // Compute checksum of header + payload
    compute_checksum(clip.data(), HEADER_SIZE + payload_size, header->sha256);

    // Write trailer (copy of checksum)
    std::memcpy(clip.data() + HEADER_SIZE + payload_size, header->sha256, TRAILER_SIZE);

    return ResultT<std::vector<uint8_t>>::ok(std::move(clip));
}

bool SyntheticClipSource::validate(const std::vector<uint8_t> &clip) {
    if (clip.size() < HEADER_SIZE + TRAILER_SIZE + MIN_PAYLOAD_SIZE)
        return false;

    const ClipHeader *header = reinterpret_cast<const ClipHeader *>(clip.data());

    // Check magic
    if (std::memcmp(header->magic, "SYNT", 4) != 0)
        return false;

    // Check version
    if (header->version != 1)
        return false;

    // Recompute checksum and compare
    uint8_t expected[32];
    compute_checksum(clip.data(), HEADER_SIZE + clip.size() - HEADER_SIZE - TRAILER_SIZE, expected);

    const ClipTrailer *trailer =
        reinterpret_cast<const ClipTrailer *>(clip.data() + HEADER_SIZE + clip.size() - HEADER_SIZE - TRAILER_SIZE);

    return std::memcmp(expected, trailer->checksum, 32) == 0;
}

void SyntheticClipSource::compute_checksum(const uint8_t *data, size_t len, uint8_t out[32]) {
    // Simple hash: SipHash-like for simulation (not crypto-grade)
    uint64_t h = 0xcbf29ce484222325ULL; // FNV-1a base

    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 0x100000001b3ULL; // FNV-1a prime
    }

    // Expand to 32 bytes
    for (int i = 0; i < 8; ++i) {
        uint64_t word = h ^ (h >> (i * 8));
        out[i * 4] = static_cast<uint8_t>(word & 0xFF);
        out[i * 4 + 1] = static_cast<uint8_t>((word >> 8) & 0xFF);
        out[i * 4 + 2] = static_cast<uint8_t>((word >> 16) & 0xFF);
        out[i * 4 + 3] = static_cast<uint8_t>((word >> 24) & 0xFF);
    }
}

void SyntheticClipSource::generate_payload(uint8_t *buf, size_t len, uint64_t seed) {
    // Deterministic pseudo-random generation using Xorshift64
    uint64_t state = seed == 0 ? 1 : seed;

    for (size_t i = 0; i < len; ++i) {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        buf[i] = static_cast<uint8_t>((state * 0x2545F4914F6CDD1DULL) >> 56);
    }
}

} // namespace camera
