#include "ota_handler.hpp"
#include "simulated_ota_port.hpp"

#include <iostream>
#include <thread>
#include <chrono>

/**
 * @brief Example real hardware port implementation.
 *
 * To target a real embedded platform, implement this interface using your
 * actual hardware drivers (Wi-Fi, Ethernet, SPI flash, etc.).
 *
 * Example platforms:
 *   - ESP32: use esp_http_client + esp_partition API
 *   - STM32: use LWIP + HAL_SPI for flash
 *   - Linux SBC: use libcurl + /dev/mtd or block device
 */
/*
class RealOTAPort : public OTAPort {
public:
    bool connect() override {
        // Initialize Wi-Fi / Ethernet, connect to update server
        // return wifi_.connect(server_url_) && http_client_.init();
        return true;  // TODO: implement
    }

    void disconnect() override {
        // Close connections, release network resources
        // http_client_.close(); wifi_.disconnect();
    }

    bool get_firmware_info(std::size_t& size,
                           std::string_view& version,
                           std::string_view& checksum) override {
        // Query server: GET /api/firmware/info
        // auto resp = http_client_.get("/firmware/info");
        // Parse JSON to extract size, version, sha256
        return true;  // TODO: implement
    }

    std::size_t read_firmware_chunk(std::byte* buffer,
                                     std::size_t max_bytes) override {
        // Read from HTTP response stream or socket
        // return http_client_.read(buffer, max_bytes);
        return 0;  // TODO: implement
    }

    bool write_flash(const std::byte* data, std::size_t size) override {
        // Write to SPI flash partition
        // return spi_flash_.write(address, data, size);
        return true;  // TODO: implement
    }

    bool erase_flash(std::size_t offset, std::size_t size) override {
        // Erase flash sector
        // return spi_flash_.erase(offset, size);
        return true;  // TODO: implement
    }

    bool verify_firmware() override {
        // Read back and compare checksum
        // auto actual = sha256(flash_region);
        // return actual == expected_checksum_;
        return true;  // TODO: implement
    }

    void mark_complete_and_reboot() override {
        // Set NVRAM flag + trigger watchdog reset
        // nvram_set("ota_success", "1");
        // system_restart();
    }
};
*/

// ── Progress display helper ──────────────────────────────────────────────────

static void print_progress(std::size_t written, std::size_t total) {
    float pct = (total > 0) ? (100.0f * written / total) : 0.0f;
    int bar_width = 30;
    int filled    = static_cast<int>(bar_width * written / total);

    std::cout << "\r  Progress: [";
    for (int i = 0; i < bar_width; ++i) {
        std::cout << (i < filled ? "█" : "░");
    }
    std::cout << "] " << static_cast<int>(pct) << "% ("
              << written << "/" << total << " bytes)";

    if (written >= total) std::cout << "\n";
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
    // ── Initialize memory pool ────────────────────────────────────────────
    MemoryPool pool;
    OTATask::set_pool(pool);

    std::cout << "Memory Pool: " << pool.capacity() << " bytes allocated.\n\n";

    // ── Create simulated OTA port ─────────────────────────────────────────
    SimulatedOTAPort port;
    port.set_chunk_size(256);   // 256-byte chunks (typical for WiFi modules)

    // ── Configure OTA handler ─────────────────────────────────────────────
    OTAConfig config;
    config.chunk_size   = 256;
    config.flash_page   = 4096;          // 4KB flash pages
    config.verify_after_write = true;
    config.on_progress  = print_progress;
    config.on_error     = [](const std::string& err) {
        std::cerr << "  ✗ ERROR: " << err << "\n";
    };
    config.on_status    = [](std::string_view status) {
        std::cout << "  [Status] " << status << "\n";
    };

    // ── Run OTA update ────────────────────────────────────────────────────
    OTAHandler handler(port, config);
    bool success = handler.run();

    // ── Report ────────────────────────────────────────────────────────────
    std::cout << "\nMemory Pool remaining: " << pool.remaining()
              << "/" << pool.capacity() << " bytes\n";

    return success ? 0 : 1;
}
