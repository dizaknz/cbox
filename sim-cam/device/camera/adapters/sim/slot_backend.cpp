#include "slot_backend.h"
#include <cstring>

namespace camera {

SimSlotBackend::SimSlotBackend() { std::memset(inactive_image_, 0, sizeof(inactive_image_)); }

Result SimSlotBackend::init(const std::string &active_version, const std::string &inactive_version) {
    // Initialize slot A as active
    slots_[0].active = true;
    slots_[0].bootable = true;
    slots_[0].version = active_version;

    // Initialize slot B as inactive
    slots_[1].active = false;
    slots_[1].bootable = false;
    slots_[1].version = inactive_version;

    active_index_ = 0;
    return Result::ok();
}

SlotInfo SimSlotBackend::get_active_slot() const { return slots_[active_index_]; }

SlotInfo SimSlotBackend::get_inactive_slot() const {
    int idx = 1 - active_index_;
    return slots_[idx];
}

ResultT<size_t> SimSlotBackend::download(const uint8_t *data, size_t len, const uint8_t sha256[32]) {
    if (!data || len == 0) {
        return ResultT<size_t>::err(make_error(Error::Code::InvalidArgument, "No data"));
    }

    int inactive_idx = 1 - active_index_;

    if (len > sizeof(inactive_image_)) {
        return ResultT<size_t>::err(make_error(Error::Code::BufferTooSmall, "Image too large"));
    }

    std::memcpy(inactive_image_, data, len);
    inactive_image_size_ = len;

    if (sha256) {
        std::memcpy(slots_[inactive_idx].sha256, sha256, 32);
    }

    slots_[inactive_idx].image_size = len;
    return ResultT<size_t>::ok(len);
}

ResultT<bool> SimSlotBackend::verify(const uint8_t expected_sha256[32]) {
    if (!expected_sha256) {
        return ResultT<bool>::err(make_error(Error::Code::InvalidArgument, "No SHA-256 provided"));
    }

    int inactive_idx = 1 - active_index_;

    // In simulation, we trust the stored sha256 from download()
    if (std::memcmp(slots_[inactive_idx].sha256, expected_sha256, 32) == 0) {
        return ResultT<bool>::ok(true);
    }

    return ResultT<bool>::ok(false);
}

Result SimSlotBackend::prepare() {
    int inactive_idx = 1 - active_index_;
    slots_[inactive_idx].bootable = true;
    return Result::ok();
}

Result SimSlotBackend::commit() {
    int inactive_idx = 1 - active_index_;

    // Switch active slot
    slots_[active_index_].active = false;
    slots_[inactive_idx].active = true;
    active_index_ = inactive_idx;

    return Result::ok();
}

Result SimSlotBackend::rollback() {
    int inactive_idx = 1 - active_index_;

    // Revert: make current inactive again, restore previous as active
    slots_[inactive_idx].bootable = false;
    slots_[active_index_].active = true;

    return Result::ok();
}

void SimSlotBackend::set_version(const std::string &version) {
    int inactive_idx = 1 - active_index_;
    slots_[inactive_idx].version = version;
}

} // namespace camera
