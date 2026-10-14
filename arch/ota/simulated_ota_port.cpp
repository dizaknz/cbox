#include "simulated_ota_port.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <random>
#include <numeric>

// ── SimulatedFirmwareBlob ────────────────────────────────────────────────────

namespace {

/**
 * @brief Generates a fake firmware image in memory for simulation.
 */
class SimulatedFirmwareBlob {
public:
    static constexpr std::size_t TOTAL_SIZE = 4096;   // 16 chunks

    explicit SimulatedFirmwareBlob(std::uint32_t seed)
        : version_("2.1.0-stable"), checksum_("d41d8cd98f00b204e9800998ecf8427e") {

        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(1, 255);

        data_.resize(TOTAL_SIZE);
        for (auto& byte : data_) {
            byte = static_cast<std::byte>(dist(rng));
        }

        // Write a simple header so we can identify it
        std::memcpy(data_.data(), "FWUP", 4);          // Magic
        std::memcpy(data_.data() + 4, &TOTAL_SIZE, 4);   // Size
    }

    const std::string& version() const { return version_; }
    const std::string& checksum() const { return checksum_; }
    std::size_t size() const { return data_.size(); }

    /**
     * @brief Compute a simple checksum over the firmware data.
     */
    static std::string compute_checksum(const std::byte* data, std::size_t len) {
        // Simple XOR + rotate checksum (not cryptographic — just for simulation)
        std::uint32_t hash = 0;
        for (std::size_t i = 0; i < len; ++i) {
            hash ^= static_cast<std::uint32_t>(data[i]);
            hash = (hash << 5) | (hash >> 27);   // Rotate left by 5
        }
        char buf[9] = {};
        std::snprintf(buf, sizeof(buf), "%08x", hash);
        return std::string(buf);
    }

    /**
     * @brief Read the next chunk at a given offset (does NOT advance cursor).
     */
    std::size_t read(std::byte* out, std::size_t max_bytes, std::size_t offset) const {
        std::size_t to_read = std::min(max_bytes, data_.size() - offset);
        if (to_read == 0) return 0;
        std::memcpy(out, data_.data() + offset, to_read);
        return to_read;
    }

public:
    const std::byte* data() const { return data_.data(); }

private:
    std::vector<std::byte> data_;
    std::string version_;
    std::string checksum_;
};

} // anonymous namespace

// ── SimulatedFirmwareStore (singleton-like) ────────────────────────────────

namespace {

/**
 * @brief Stores a single firmware blob shared across all simulated ports.
 */
class SimulatedFirmwareStore {
public:
    static SimulatedFirmwareStore& instance() {
        static SimulatedFirmwareStore store;
        return store;
    }

    const SimulatedFirmwareBlob& get_blob() const { return *blob_; }

private:
    SimulatedFirmwareStore()
        : blob_(std::make_unique<SimulatedFirmwareBlob>(42)) {}

    std::unique_ptr<SimulatedFirmwareBlob> blob_;
};

} // anonymous namespace

// ── SimulatedOTAPort implementation ────────────────────────────────────────

SimulatedOTAPort::SimulatedOTAPort()
    : connected_(false), cursor_(0) {}

bool SimulatedOTAPort::connect() {
    if (connected_) return true;

    std::cout << "  [SimPort] Connecting to OTA server...\n";
    // Simulate network latency
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    connected_ = true;
    cursor_ = 0;
    std::cout << "  [SimPort] Connected.\n";
    return true;
}

void SimulatedOTAPort::disconnect() {
    if (!connected_) return;

    std::cout << "  [SimPort] Disconnecting from OTA server...\n";
    connected_ = false;
    cursor_ = 0;
    std::cout << "  [SimPort] Disconnected.\n";
}

bool SimulatedOTAPort::get_firmware_info(std::size_t& size,
                                          std::string& version,
                                          std::string& checksum) {
    if (!connected_) return false;

    const auto& blob = SimulatedFirmwareStore::instance().get_blob();
    size       = blob.size();
    version    = blob.version();
    checksum   = blob.checksum();

    std::cout << "  [SimPort] Firmware info: v" << version
              << ", " << size << " bytes, checksum=" << checksum << "\n";
    return true;
}

std::size_t SimulatedOTAPort::read_firmware_chunk(std::byte* buffer,
                                                   std::size_t max_bytes) {
    if (!connected_) return 0;

    const auto& blob = SimulatedFirmwareStore::instance().get_blob();
    std::size_t to_read = std::min(max_bytes, chunk_size_);

    std::size_t read = blob.read(buffer, to_read, cursor_);
    cursor_ += read;

    if (read == 0) {
        std::cout << "  [SimPort] No more firmware data (EOF).\n";
    } else {
        std::cout << "  [SimPort] Read " << read << " bytes (offset=" << cursor_ << ").\n";
    }

    return read;
}

bool SimulatedOTAPort::write_flash(const std::byte* data, std::size_t size) {
    // Simulate flash write latency (proportional to size)
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<unsigned>(size / 64)));

    // Simulate occasional write errors (1% chance when testing error handling)
    static std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, 99);
    if (dist(rng) == 0 && size > 0) {
        std::cout << "  [SimPort] ⚠ Flash write error at offset " << flash_offset_
                  << "!\n";
        return false;
    }

    flash_data_.insert(flash_data_.end(), data, data + size);
    flash_offset_ += size;

    std::cout << "  [SimPort] Wrote " << size << " bytes to flash (offset="
              << flash_offset_ - size << ").\n";
    return true;
}

bool SimulatedOTAPort::erase_flash(std::size_t offset, std::size_t size) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // In simulation, just clear the range
    if (offset < flash_data_.size()) {
        auto start = flash_data_.begin() + static_cast<std::ptrdiff_t>(offset);
        auto end   = std::min(flash_data_.end(),
                              start + static_cast<std::ptrdiff_t>(size));
        std::fill(start, end, std::byte{0xFF});
    }

    std::cout << "  [SimPort] Erased " << size << " bytes at offset " << offset << ".\n";
    return true;
}

bool SimulatedOTAPort::verify_firmware() {
    const auto& blob = SimulatedFirmwareStore::instance().get_blob();
    std::string expected = SimulatedFirmwareBlob::compute_checksum(
        blob.data(), blob.size());

    std::string actual = SimulatedFirmwareBlob::compute_checksum(
        flash_data_.data(), flash_data_.size());

    bool match = (expected == actual);
    std::cout << "  [SimPort] Verify: expected=" << expected
              << ", actual=" << actual
              << ", result=" << (match ? "PASS" : "FAIL") << "\n";
    return match;
}

void SimulatedOTAPort::mark_complete_and_reboot() {
    std::cout << "  [SimPort] ✓ OTA update successful! Rebooting...\n";
    // In simulation, just print — real port would trigger a reset
    std::cout << "  [SimPort] (In production this triggers a hardware reset.)\n";
}
