# OTA Firmware Update Framework

A C++20 coroutine-based Over-The-Air (OTA) firmware update framework designed for embedded systems with deterministic memory allocation — **zero heap usage**.

## Features

- **Coroutine-driven state machine** — clean, sequential code that naturally models the download → write → verify lifecycle
- **Static memory pool** — all allocations come from a pre-allocated BSS buffer, no `new`/`delete` on the system heap
- **Pluggable hardware ports** — swap between simulated and real implementations via a single abstract interface
- **Event-loop integration** — each firmware chunk is processed on resume, making it trivial to hook into an RTOS scheduler, interrupt handler, or async event loop
- **Progress & error callbacks** — pluggable handlers for UI updates, logging, and failure recovery

## Project Structure

```
ota/
├── CMakeLists.txt            # Build configuration (C++20)
├── main.cpp                  # Entry point + RealOTAPort implementation template
├── memory_pool.hpp           # Static RAM pool with PMR allocator
├── ota_port.hpp              # Abstract OTA hardware interface
├── ota_handler.hpp           # OTATask coroutine wrapper + OTAHandler orchestrator
├── ota_handler.cpp           # Coroutine-based download & flash-write logic
├── simulated_ota_port.hpp    # Simulated port header (desktop testing)
└── simulated_ota_port.cpp    # Simulated port implementation
```

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                   OTAHandler                        │
│  connect → get_info → erase_flash → verify          │
└────────────────────────┬────────────────────────────┘
                         │ drives coroutine step-by-step
                         ▼
┌─────────────────────────────────────────────────────┐
│              download_firmware() coroutine           │
│  read_chunk → write_flash → erase_next_page → yield │
└────────────────────────┬────────────────────────────┘
                         │ calls through interface
                         ▼
┌─────────────────────────────────────────────────────┐
│                   OTAPort (abstract)                │
│         ╭──────────────┴──────────────╮             │
│         ▼                              ▼             │
│  SimulatedOTAPort              RealOTAPort           │
│  (desktop test)                (ESP32/STM32/Linux)   │
└─────────────────────────────────────────────────────┘
```

## Building

### Prerequisites

- C++20 compiler (GCC ≥ 13, Clang ≥ 14, or MSVC with `/await`)
- CMake ≥ 3.20

### Quick build

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Run the simulator

```bash
./ota_server
```

Output:

```
╔══════════════════════════════════════╗
║       OTA Firmware Update Start      ║
╚══════════════════════════════════════╝

  [Status] connecting
  [SimPort] Connecting to OTA server...
  [SimPort] Connected.
  [Status] fetching_info
  [SimPort] Firmware info: v2.1.0-stable, 4096 bytes, checksum=...
  ...
  Progress: [██████████████████████████████] 100% (4096/4096 bytes)
  ...
╔══════════════════════════════════════╗
║       OTA Update Successful!         ║
╚══════════════════════════════════════╝
```

## Using the Simulator

The `SimulatedOTAPort` generates a fake firmware blob (4 KB by default) and simulates:

| Simulation | Detail |
|---|---|
| **Network latency** | 50 ms on connect, proportional delay on reads |
| **Flash write latency** | Scales with chunk size (simulated SPI flash) |
| **Random write errors** | ~1% chance per write — tests error handling paths |
| **Checksum verification** | XOR+rotate hash compared against stored value |

This lets you test the full OTA flow on a desktop without any hardware.

## Implementing a Real Hardware Port

Create a class that inherits from `OTAPort` and implements all pure virtual methods:

```cpp
class RealOTAPort : public OTAPort {
public:
    bool connect() override {
        // Initialize Wi-Fi/Ethernet, connect to update server
        return wifi_.connect(server_url_) && http_client_.init();
    }

    void disconnect() override {
        http_client_.close();
        wifi_.disconnect();
    }

    bool get_firmware_info(std::size_t& size,
                           std::string& version,
                           std::string& checksum) override {
        // GET /api/firmware/info → parse JSON
        auto resp = http_client_.get("/firmware/info");
        // Extract size, version, sha256 from response
        return true;
    }

    std::size_t read_firmware_chunk(std::byte* buffer,
                                     std::size_t max_bytes) override {
        // Read from HTTP stream or raw socket
        return http_client_.read(buffer, max_bytes);
    }

    bool write_flash(const std::byte* data, std::size_t size) override {
        // Write to SPI flash partition
        return spi_flash_.write(address_, data, size);
    }

    bool erase_flash(std::size_t offset, std::size_t size) override {
        return spi_flash_.erase(offset, size);
    }

    bool verify_firmware() override {
        auto actual = sha256(flash_region_);
        return actual == expected_checksum_;
    }

    void mark_complete_and_reboot() override {
        nvram_set("ota_success", "1");
        system_restart();
    }

private:
    WiFiInterface      wifi_;
    HttpClient         http_client_;
    SpiFlashDriver     spi_flash_;
    std::string        expected_checksum_;
    std::size_t        address_ = 0;   // Current flash write offset
};
```

A full commented template is included in `main.cpp` — just uncomment and fill in the blanks.

## Configuration

The `OTAConfig` struct controls update behaviour:

```cpp
OTAConfig config;
config.chunk_size           = 256;       // Bytes per read/write (typical for WiFi)
config.flash_page           = 4096;      // Flash page size to erase before writing
config.verify_after_write   = true;      // Verify each page after writing

// Callbacks
config.on_progress = [](std::size_t written, std::size_t total) {
    // Update UI progress bar
};
config.on_error    = [](const std::string& err) {
    // Log error, trigger recovery
};
config.on_status   = [](std::string_view status) {
    // Emit status event to UI / log
};
```

## Memory Model

All coroutine frames are allocated from a static `MemoryPool`:

```cpp
MemoryPool pool;                    // 2 KB static buffer (BSS segment)
OTATask::set_pool(pool);            // Route all OTATask allocations through it
```

The pool tracks allocations atomically so you can monitor remaining memory:

```cpp
std::cout << "Remaining: " << pool.remaining() << "/" << pool.capacity() << "\n";
```

No dynamic allocation occurs after `main()` starts — safe for bare-metal and RTOS environments.

## Coroutine Flow

```
OTAHandler::run()
  ├─ port.connect()
  ├─ port.get_firmware_info()
  ├─ port.erase_flash() × N pages
  │
  ├─ download_firmware() coroutine ───────────────┐
  │   loop:                                       │
  │     chunk = port.read_firmware_chunk()        │
  │     if (chunk == 0) break;                    │
  │     port.write_flash(chunk)                   │
  │     port.erase_flash(next_page)               │
  │     co_await suspend_always{}   ◄── event loop│
  │                                               │
  └─ port.verify_firmware()                      │
    ├─ port.mark_complete_and_reboot()            │
    └─ port.disconnect()                          │
```

Each `co_await` yields control back to the caller (your event loop / RTOS task), which resumes the coroutine when the next hardware event fires (e.g., DMA complete, socket data available).

## License

MIT
