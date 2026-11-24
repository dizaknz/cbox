#pragma once
// slot_backend.h — SimSlotBackend: simulates A/B OTA partition slots
// Replaces real eMMC/UFS partitions with in-memory storage.

#include "generated/envelope_generated.h"
#include "generated/ota_generated.h"
#include "value/result.h"
#include <cstdint>
#include <string>
#include <vector>

namespace camera {

// ── SimSlotBackend ───────────────────────────────────────────────
// Manages two OTA slots (A and B) with firmware images stored in memory.
// Simulates download, verification, installation, and rollback.

struct SlotInfo {
    bool active = false;
    bool bootable = false;
    std::string version;
    size_t image_size = 0;
    uint8_t sha256[32] = {};
};

class SimSlotBackend {
  public:
    SimSlotBackend();

    // Initialize slots (A and B)
    Result init(const std::string &active_version, const std::string &inactive_version);

    // Get slot info
    SlotInfo get_active_slot() const;
    SlotInfo get_inactive_slot() const;

    // Download an image to the inactive slot. Returns size downloaded.
    ResultT<size_t> download(const uint8_t *data, size_t len, const uint8_t sha256[32]);

    // Verify the downloaded image against expected SHA-256
    ResultT<bool> verify(const uint8_t expected_sha256[32]);

    // Mark the inactive slot as bootable (prepare for next boot)
    Result prepare();

    // Commit: permanently switch to the new slot
    Result commit();

    // Rollback: revert to the previous active slot
    Result rollback();

    // Set firmware version for a slot
    void set_version(const std::string &version);

  private:
    SlotInfo slots_[2]; // [0] = A, [1] = B
    int active_index_ = 0;
    uint8_t inactive_image_[64 * 1024 * 1024]; // 64 MB max per slot
    size_t inactive_image_size_ = 0;
};

} // namespace camera
