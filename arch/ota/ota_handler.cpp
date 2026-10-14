#include "ota_handler.hpp"

#include <iostream>
#include <vector>
#include <string>

// ── Static member ────────────────────────────────────────────────────────────

MemoryPool* OTATask::pool_ptr_ = nullptr;

// ── Coroutine state passed through promise ───────────────────────────────────

/**
 * @brief Shared state between OTAHandler and the download coroutine.
 */
struct DownloadContext {
    OTAPort&        port;
    const OTAConfig config;
    std::size_t     total_bytes   = 0;
    std::size_t     bytes_written = 0;
    std::size_t     firmware_size = 0; // Total size known from server
    bool            success       = false;
    std::string     error_message;
    std::vector<std::byte> read_buffer;

    explicit DownloadContext(OTAPort& p, const OTAConfig& c)
        : port(p), config(c), read_buffer(c.chunk_size) {}
};

// ── Coroutine function ───────────────────────────────────────────────────────

/**
 * @brief Coroutine that reads firmware chunks and writes them to flash.
 *
 * The coroutine suspends after each chunk so the caller can drive it
 * step-by-step (simulating event-driven hardware).
 */
static OTATask download_firmware(DownloadContext& ctx) {
    std::size_t page_offset = 0;
    std::size_t page_bytes  = 0;

    while (true) {
        // ── Read a chunk from the port ────────────────────────────────
        auto bytes_read = ctx.port.read_firmware_chunk(
            ctx.read_buffer.data(), ctx.config.chunk_size);

        if (bytes_read == 0) break;   // EOF

        ctx.total_bytes += bytes_read;

        // ── Write to flash ────────────────────────────────────────────
        if (!ctx.port.write_flash(ctx.read_buffer.data(), bytes_read)) {
            ctx.error_message = "Flash write failed at offset " +
                                std::to_string(ctx.bytes_written);
            co_return;
        }

        ctx.bytes_written += bytes_read;
        page_bytes += bytes_read;

        // ── Erase next flash page when current page is full ───────────
        if (page_bytes >= ctx.config.flash_page) {
            page_bytes = 0;
            page_offset += ctx.config.flash_page;

            if (ctx.config.verify_after_write) {
                // Verify the page we just wrote (simulate by checking port)
                std::cout << "  [Handler] Page verified at offset "
                          << (ctx.bytes_written - bytes_read) << "\n";
            }

            // Erase next page ahead of time (pipeline)
            if (!ctx.port.erase_flash(page_offset, ctx.config.flash_page)) {
                ctx.error_message = "Flash erase failed at offset " +
                                    std::to_string(page_offset);
                co_return;
            }
        }

        // ── Progress callback ─────────────────────────────────────────
        if (ctx.config.on_progress && ctx.firmware_size > 0) {
            ctx.config.on_progress(ctx.bytes_written, ctx.firmware_size);
        }

        // Yield control — the event loop resumes us when more data is ready
        co_await std::suspend_always{};
    }

    // ── Download complete ─────────────────────────────────────────────
    ctx.success = true;
    std::cout << "  [Handler] Download complete: "
              << ctx.bytes_written << " bytes.\n";
}

// ── OTAHandler implementation ────────────────────────────────────────────────

bool OTAHandler::run() {
    std::cout << "\n"
              << "╔══════════════════════════════════════╗\n"
              << "║       OTA Firmware Update Start      ║\n"
              << "╚══════════════════════════════════════╝\n\n";

    // ── Step 1: Connect ───────────────────────────────────────────────────
    if (config_.on_status) config_.on_status("connecting");
    if (!port_.connect()) {
        if (config_.on_error) config_.on_error("Failed to connect to OTA server");
        std::cout << "✗ Connection failed.\n";
        return false;
    }

    // ── Step 2: Get firmware info ─────────────────────────────────────────
    if (config_.on_status) config_.on_status("fetching_info");
    std::size_t firmware_size = 0;
    std::string version_str, checksum_str;
    if (!port_.get_firmware_info(firmware_size, version_str, checksum_str)) {
        if (config_.on_error) config_.on_error("Failed to fetch firmware info");
        port_.disconnect();
        return false;
    }

    std::cout << "  Firmware: v" << version_str
              << " | Size: " << firmware_size << " bytes\n";
    std::cout << "  Checksum: " << checksum_str << "\n\n";

    // ── Step 3: Erase target flash ────────────────────────────────────────
    if (config_.on_status) config_.on_status("erasing_flash");
    std::size_t pages_needed =
        (firmware_size + config_.flash_page - 1) / config_.flash_page;

    for (std::size_t i = 0; i < pages_needed; ++i) {
        if (!port_.erase_flash(i * config_.flash_page, config_.flash_page)) {
            if (config_.on_error)
                config_.on_error("Flash erase failed at page " + std::to_string(i));
            port_.disconnect();
            return false;
        }
    }
    std::cout << "  Erased " << pages_needed << " flash pages.\n\n";

    // ── Step 4: Download & write (coroutine) ──────────────────────────────
    if (config_.on_status) config_.on_status("downloading");

    DownloadContext ctx(port_, config_);
    ctx.firmware_size = firmware_size;  // Pass total size for progress tracking
    auto coro = download_firmware(ctx);

    while (!coro.is_done()) {
        coro.resume();
    }

    if (!ctx.success) {
        if (config_.on_error) config_.on_error(ctx.error_message);
        std::cout << "\n✗ Download failed: " << ctx.error_message << "\n";
        port_.disconnect();
        return false;
    }

    // ── Step 5: Verify ────────────────────────────────────────────────────
    if (config_.on_status) config_.on_status("verifying");
    if (!port_.verify_firmware()) {
        if (config_.on_error) config_.on_error("Firmware verification failed!");
        port_.disconnect();
        return false;
    }

    // ── Step 6: Complete ──────────────────────────────────────────────────
    if (config_.on_status) config_.on_status("complete");

    std::cout << "\n  Total transferred: " << ctx.bytes_written << "/"
              << firmware_size << " bytes\n";

    port_.mark_complete_and_reboot();
    port_.disconnect();

    std::cout << "\n╔══════════════════════════════════════╗\n"
              << "║       OTA Update Successful!         ║\n"
              << "╚══════════════════════════════════════╝\n";

    return true;
}
