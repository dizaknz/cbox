#pragma once
// eventd.h — Event policy engine: receives SensorTriggered, applies cooldown/suppression,
// mints event IDs, and produces EventRaised messages.

#include "bus/endpoint.h"
#include "event_source.h"
#include "generated/envelope_generated.h"
#include "generated/events_generated.h"
#include "reactor/reactor.h"
#include "value/result.h"
#include "value/uuid.h"
#include <cstdint>
#include <string>
#include <unordered_map>

using namespace camera::v1;

namespace camera {

// ── Eventd configuration ─────────────────────────────────────────

struct EventdConfig {
    uint16_t sensor_id = 1;
    SensorKind sensor_kind = SensorKind_Motion;
    uint32_t cooldown_s = 30;           // seconds between events of same type
    double suppression_threshold = 5.0; // suppress after N triggers in cooldown window
    std::string device_id_str;          // hex string of device UUID
    std::string tenant_id_str;          // hex string of tenant UUID

    // Bus configuration
    std::string listen_path;         // Unix socket path for listening
    std::string eventd_connect_path; // path to connect to (if acting as client)

    // Sim config
    EventSourceConfig sim_config;
};

// ── Eventd ───────────────────────────────────────────────────────

class Eventd {
  public:
    explicit Eventd(Reactor &reactor);
    ~Eventd();

    // Initialize with configuration (must be called before run())
    Result init(const EventdConfig &config);

    // Start the event daemon (bus + sim source)
    void start();

    // Stop all operations
    void stop();

    // Run the reactor event loop (blocks until stop())
    void run();

  private:
    // ── Bus message handlers ────────────────────────────────────
    void onSensorTriggered(const Envelope *env, const uint8_t *data);
    void onCaptureNow(const Envelope *env, const uint8_t *data);
    void onConfigUpdated(const Envelope *env, const uint8_t *data);

    // ── Sim event handler ───────────────────────────────────────
    void onSimEvent(flatbuffers::FlatBufferBuilder &envelope);

    // ── Policy engine ───────────────────────────────────────────
    bool should_suppress(SensorKind kind);
    void record_event(SensorKind kind);
    flatbuffers::FlatBufferBuilder
    buildRaisedEnvelope(const SensorTriggered *triggered, SensorKind kind, uint16_t suppressed_count);

    Reactor &reactor_;
    EventdConfig config_;
    BusEndpoint bus_;
    SimEventSource sim_source_;

    // Policy state: track last event time per sensor+kind
    struct CooldownState {
        uint64_t last_event_ns = 0;
        uint32_t suppressed_count = 0;
    };
    std::unordered_map<uint16_t, CooldownState> cooldowns_;

    // Device/tenant UUIDs
    UuidV7 device_uuid_;
    UuidV7 tenant_uuid_;
    UuidV7 boot_uuid_;

    bool running_ = false;
};

} // namespace camera
