#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <string>
#include <optional>
#include <chrono>

/**
 * @brief Abstract interface for OTA hardware ports.
 *
 * Implement this to target real hardware (Wi-Fi, Ethernet, BLE) or use
 * SimulatedOTAPort for testing on a desktop/laptop.
 */
class OTAPort {
public:
    virtual ~OTAPort() = default;

    // ── Connection lifecycle ──────────────────────────────────────────────

    /**
     * @brief Connect to the OTA server / update endpoint.
     * @return true on success, false on failure.
     */
    virtual bool connect() = 0;

    /**
     * @brief Disconnect from the OTA server.
     */
    virtual void disconnect() = 0;

    // ── Firmware metadata ────────────────────────────────────────────────

    /**
     * @brief Query firmware image info (size, version, checksum).
     * @param[out] size       Size of the firmware blob in bytes.
     * @param[out] version     Version string of the firmware.
     * @param[out] checksum    SHA-256 or MD5 hex digest.
     * @return true if metadata was retrieved successfully.
     */
    virtual bool get_firmware_info(std::size_t& size,
                                   std::string& version,
                                   std::string& checksum) = 0;

    // ── Data transfer ────────────────────────────────────────────────────

    /**
     * @brief Read the next chunk of firmware data.
     * @param[out] buffer      Destination buffer.
     * @param[in]  max_bytes   Maximum bytes to read.
     * @return Number of bytes actually read (0 = EOF / error).
     */
    virtual std::size_t read_firmware_chunk(std::byte* buffer,
                                            std::size_t max_bytes) = 0;

    // ── Flash operations ─────────────────────────────────────────────────

    /**
     * @brief Write a chunk to the flash partition.
     * @param data       Data to write.
     * @param size       Number of bytes.
     * @return true on success.
     */
    virtual bool write_flash(const std::byte* data, std::size_t size) = 0;

    /**
     * @brief Erase the target flash sector before writing.
     * @param offset     Start offset in flash.
     * @param size       Number of bytes to erase.
     * @return true on success.
     */
    virtual bool erase_flash(std::size_t offset, std::size_t size) = 0;

    /**
     * @brief Verify the written firmware against its checksum.
     * @return true if verification passes.
     */
    virtual bool verify_firmware() = 0;

    /**
     * @brief Mark the firmware update as successful and request reboot.
     */
    virtual void mark_complete_and_reboot() = 0;

    // ── Configuration ────────────────────────────────────────────────────

    /**
     * @brief Set the chunk size for firmware reads (default 256 bytes).
     */
    virtual void set_chunk_size(std::size_t size) { chunk_size_ = size; }

    std::size_t get_chunk_size() const { return chunk_size_; }

protected:
    std::size_t chunk_size_ = 256;   // Default read/write chunk size
};
