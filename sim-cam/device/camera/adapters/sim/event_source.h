#pragma once
// event_source.h — SimEventSource: generates events from scenario YAML config
// Replaces physical sensor input (GPIO, etc.) with deterministic event streams.

#include "generated/common_generated.h"
#include "generated/envelope_generated.h"
#include "generated/events_generated.h"
#include "reactor/reactor.h"
#include "value/result.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace camera::v1;

namespace camera {

// ── Event source configuration ───────────────────────────────────

struct EventSourceConfig {
    uint16_t sensor_id = 1;
    SensorKind kind = SensorKind_Motion;
    double rate_per_hour = 10.0; // average events per hour
    double burst_factor = 1.5;   // multiplier for burst events (1.0 = uniform)
    uint32_t jitter_ms = 500;    // random jitter on timing
    bool enabled = true;
};

// ── SimEventSource ───────────────────────────────────────────────
// Generates SensorTriggered messages at a configurable rate.
// Uses exponential inter-arrival times with optional burst clusters.

class SimEventSource {
  public:
    explicit SimEventSource(Reactor &reactor);
    ~SimEventSource() = default;

    // Configure the event source (must be called before start())
    Result configure(const EventSourceConfig &config);

    // Start generating events (non-blocking)
    void start();

    // Stop generating events
    void stop();

    // Set the callback for generated SensorTriggered events.
    // The callback receives a flatbuffers::FlatBufferBuilder containing the serialized Envelope.
    using EventCallback = std::function<void(flatbuffers::FlatBufferBuilder &envelope)>;
    void onEvent(EventCallback cb);

    // Generate a single event immediately (for testing)
    void generateOne();

  private:
    void scheduleNext();
    void onTimer();
    flatbuffers::FlatBufferBuilder buildTriggeredEnvelope(const EventSourceConfig &config);

    Reactor &reactor_;
    EventSourceConfig config_;
    TimerId timer_ = 0;
    bool running_ = false;
    EventCallback event_cb_;
    bool has_callback_ = false;

    // State for burst simulation
    uint32_t events_in_burst_ = 0;
    uint32_t remaining_burst_ = 0;
};

} // namespace camera
