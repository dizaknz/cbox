#include "event_source.h"
#include "value/clock_helpers.h"
#include "value/uuid.h"
#include <chrono>
#include <cmath>
#include <random>

namespace camera {

SimEventSource::SimEventSource(Reactor &reactor) : reactor_(reactor) {}

Result SimEventSource::configure(const EventSourceConfig &config) {
    if (config.rate_per_hour <= 0) {
        return Result::err(make_error(Error::Code::InvalidArgument, "rate_per_hour must be > 0"));
    }
    config_ = config;
    return Result::ok();
}

void SimEventSource::start() {
    if (running_)
        return;
    running_ = true;
    events_in_burst_ = 0;
    remaining_burst_ = 0;
    scheduleNext();
}

void SimEventSource::stop() {
    running_ = false;
    if (timer_) {
        reactor_.cancelTimer(timer_);
        timer_ = 0;
    }
}

void SimEventSource::onEvent(EventCallback cb) {
    event_cb_ = std::move(cb);
    has_callback_ = true;
}

void SimEventSource::generateOne() {
    if (!has_callback_)
        return;
    flatbuffers::FlatBufferBuilder fbb = buildTriggeredEnvelope(config_);
    event_cb_(fbb);
}

void SimEventSource::scheduleNext() {
    if (!running_ || !has_callback_)
        return;

    // Calculate inter-arrival time
    double base_interval_ms = 3600000.0 / config_.rate_per_hour; // ms between events

    // Handle burst: when burst_factor > 1, cluster events together
    if (config_.burst_factor > 1.0 && remaining_burst_ == 0) {
        // Decide whether to start a burst (probability proportional to burst_factor)
        static thread_local std::mt19937 rng(std::random_device{}());
        double prob = 1.0 / config_.burst_factor;
        std::uniform_real_distribution<double> dist(0, 1);

        if (dist(rng) < prob) {
            // Start a burst: generate 2-5 events close together
            remaining_burst_ = 2 + static_cast<uint32_t>(dist(rng) * 3);
            base_interval_ms /= config_.burst_factor; // Speed up during burst
        }
    }

    if (remaining_burst_ > 0) {
        remaining_burst_--;
        if (remaining_burst_ == 0) {
            base_interval_ms *= config_.burst_factor; // Slow down after burst
        }
    }

    // Add jitter: ±jitter_ms around the mean interval
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> jitter_dist(
        -static_cast<double>(config_.jitter_ms), static_cast<double>(config_.jitter_ms)
    );
    double interval = base_interval_ms + jitter_dist(rng);
    if (interval < 10.0)
        interval = 10.0; // Minimum 10ms

    timer_ = reactor_.addTimer(static_cast<uint64_t>(interval), [this]() { onTimer(); });
}

void SimEventSource::onTimer() {
    if (!running_)
        return;

    generateOne();
    scheduleNext();
}

flatbuffers::FlatBufferBuilder SimEventSource::buildTriggeredEnvelope(const EventSourceConfig &config) {
    flatbuffers::FlatBufferBuilder fbb;

    // Build SensorTriggered payload
    auto observed = make_clock(monotonic_ns(), wall_ms());

    camera::v1::SensorTriggeredBuilder triggered(fbb);
    triggered.add_sensor_id(config.sensor_id);
    triggered.add_kind(config.kind);
    triggered.add_observed(&observed);
    triggered.add_edge_count(1);
    auto payload = triggered.Finish();

    // Build Envelope wrapper
    auto msg_id = generate_uuid_v7();
    auto corr_id = generate_uuid_v7(); // correlation: same as event_id for standalone triggers
    auto device_id = generate_uuid_v7();
    auto tenant_id = generate_uuid_v7();
    auto boot_id = generate_uuid_v7();

    auto msg_id_fb = to_fb_uuid(msg_id);
    auto corr_id_fb = to_fb_uuid(corr_id);
    auto device_id_fb = to_fb_uuid(device_id);
    auto tenant_id_fb = to_fb_uuid(tenant_id);
    auto boot_id_fb = to_fb_uuid(boot_id);

    EnvelopeBuilder env(fbb);
    env.add_schema_version(1);
    env.add_message_id(&msg_id_fb);
    env.add_correlation_id(&corr_id_fb);
    env.add_device_id(&device_id_fb);
    env.add_tenant_id(&tenant_id_fb);
    env.add_boot_id(&boot_id_fb);
    auto clock_synced = make_clock(monotonic_ns(), wall_ms());
    env.add_sent(&clock_synced);
    env.add_clock_synced(true);
    env.add_priority(Priority_P1Normal);
    env.add_source(ServiceId_Sensord);
    env.add_payload_type(Payload_SensorTriggered);
    env.add_payload(payload.Union());
    auto envelope = env.Finish();

    fbb.Finish(envelope);
    return fbb;
}

} // namespace camera
