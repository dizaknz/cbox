#pragma once

#include "ota_port.hpp"

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <chrono>

/**
 * @brief Simulated OTA port for desktop testing.
 *
 * Generates a fake firmware blob, simulates network reads and flash writes
 * with realistic timing and occasional errors. Use this to test the OTA
 * handler without any hardware.
 */
class SimulatedOTAPort : public OTAPort {
public:
    SimulatedOTAPort();

    // Connection lifecycle
    bool connect() override;
    void disconnect() override;

    // Firmware metadata
    bool get_firmware_info(std::size_t& size,
                           std::string& version,
                           std::string& checksum) override;

    // Data transfer
    std::size_t read_firmware_chunk(std::byte* buffer,
                                    std::size_t max_bytes) override;

    // Flash operations
    bool write_flash(const std::byte* data, std::size_t size) override;
    bool erase_flash(std::size_t offset, std::size_t size) override;
    bool verify_firmware() override;
    void mark_complete_and_reboot() override;

private:
    bool connected_              = false;
    std::size_t cursor_          = 0;   // Read cursor in simulated firmware blob
    std::size_t flash_offset_    = 0;   // Write cursor in simulated flash
    std::vector<std::byte> flash_data_; // Simulated flash storage
};
