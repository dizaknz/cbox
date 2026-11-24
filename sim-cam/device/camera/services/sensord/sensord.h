#pragma once
// sensord — Simulated sensor daemon
// Generates debounced GPIO-style sensor triggers and publishes them
// to eventd via the bus (SOCK_SEQPACKET Unix sockets).

#include "bus/endpoint.h"
#include "reactor/reactor.h"
#include "clock.h"
#include "value/result.h"
#include <chrono>
#include <cstdint>
#include <string>

namespace camera {

// ── sensord configuration ────────────────────────────────────────

struct SensordConfig {
    uint16_t sensor_id = 1;
    ServiceId kind = ServiceId_Sensord;
    double event_rate_per_hour = 10.0;   // average events per hour (Poisson)
    uint32_t debounce_ms = 500;          // minimum interval between triggers
    std::string device_id_str;           // 32-char hex UUID
    std::string tenant_id_str;           // 32-char hex UUID
    std::string eventd_path;             // Unix socket path to connect to
};

// ── Sensord ──────────────────────────────────────────────────────
// Simulates GPIO debouncing: generates events at a configurable rate,
// enforces a debounce interval, and publishes SensorTriggered messages
// to eventd via the bus.

class Sensord {
  public:
    Sensord(Reactor &reactor);
    ~Sensord();

    // Initialize with configuration. Returns Error on failure.
    Result init(const SensordConfig &config);

    // Start the sensor simulation loop.
    void start();

    // Run the reactor event loop (blocks until stopped).
    void run();

  private:
    // Schedule the next sensor trigger based on Poisson distribution.
    void scheduleNextTrigger();

    // Called when it's time to fire a sensor trigger.
    void onTriggerFired();

    // Build and send a SensorTriggered message to eventd.
    Result sendTrigger();

    // Handle connection to eventd.
    Result connectToEventd();

    // Handle successful connection to eventd.
    void onConnected();

    // Handle bus messages (we expect acks from eventd).
    void onBusMessage(const Envelope *env, const uint8_t *data);

    Reactor &reactor_;
    SimClock clock_;

    SensordConfig config_;
    BusEndpoint bus_;

    // Timing state
    uint64_t last_trigger_mono_ns_ = 0;
    TimerId trigger_timer_ = 0;
    bool running_ = false;
    bool connected_ = false;

    // Statistics
    uint32_t triggers_sent_ = 0;
    uint32_t triggers_suppressed_ = 0;
};

} // namespace camera
