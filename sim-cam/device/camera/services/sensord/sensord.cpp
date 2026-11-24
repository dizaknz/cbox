// sensord — Simulated sensor daemon
// Generates debounced GPIO-style sensor triggers and publishes them
// to eventd via the bus (SOCK_SEQPACKET Unix sockets).

#include "sensord/sensord.h"
#include "generated/events_generated.h"
#include "value/uuid.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>

using namespace camera;

// ── Constructors / Destructor ────────────────────────────────────

Sensord::Sensord(Reactor &reactor)
    : reactor_(reactor), clock_(SimClock::realtime()), bus_(reactor, ServiceId_Sensord) {
}

Sensord::~Sensord() {
    if (trigger_timer_ != 0) {
        reactor_.cancelTimer(trigger_timer_);
        trigger_timer_ = 0;
    }
}

// ── Public API ───────────────────────────────────────────────────

Result Sensord::init(const SensordConfig &config) {
    config_ = config;

    // Generate UUIDs if not provided
    if (config_.device_id_str.empty()) {
        auto id = generate_uuid_v7();
        std::ostringstream oss;
        for (int j = 0; j < 8; ++j) {
            oss << std::hex << std::setfill('0') << std::setw(2) << ((id.hi >> (56 - j * 8)) & 0xFF);
        }
        for (int j = 0; j < 8; ++j) {
            oss << std::hex << std::setfill('0') << std::setw(2) << ((id.lo >> (56 - j * 8)) & 0xFF);
        }
        config_.device_id_str = oss.str();
    }

    if (config_.tenant_id_str.empty()) {
        auto id = generate_uuid_v7();
        std::ostringstream oss;
        for (int j = 0; j < 8; ++j) {
            oss << std::hex << std::setfill('0') << std::setw(2) << ((id.hi >> (56 - j * 8)) & 0xFF);
        }
        for (int j = 0; j < 8; ++j) {
            oss << std::hex << std::setfill('0') << std::setw(2) << ((id.lo >> (56 - j * 8)) & 0xFF);
        }
        config_.tenant_id_str = oss.str();
    }

    // Set default eventd socket path if not provided
    if (config_.eventd_path.empty()) {
        config_.eventd_path = "/tmp/eventd.sock";
    }

    // Register bus message handler (for acks from eventd)
    bus_.onPayload(Payload_EventRaised, [this](const Envelope *env, const uint8_t *data) {
        (void)env;
        (void)data;
        // We could process acks here if needed
    });

    return Result::ok();
}

void Sensord::start() {
    if (running_) {
        std::cerr << "sensord: already running\n";
        return;
    }

    running_ = true;
    connected_ = false;
    last_trigger_mono_ns_ = 0;
    triggers_sent_ = 0;
    triggers_suppressed_ = 0;

    // Set up connection callback
    bus_.onConnected([this](int peer_fd) {
        (void)peer_fd;
        reactor_.post([this]() { onConnected(); });
    });

    // Connect to eventd asynchronously
    connectToEventd();
}

void Sensord::run() {
    reactor_.run();
    running_ = false;
}

// ── Private Implementation ───────────────────────────────────────

void Sensord::scheduleNextTrigger() {
    // Poisson process: inter-arrival time is exponentially distributed
    double rate_per_second = config_.event_rate_per_hour / 3600.0;
    if (rate_per_second <= 0.0) {
        rate_per_second = 1.0 / 3600.0; // minimum 1 per hour
    }

    // Generate exponential random variable: -ln(U) / lambda
    static thread_local std::mt19937 rng(42); // deterministic seed for simulation
    std::exponential_distribution<double> dist(rate_per_second);
    double interval_seconds = dist(rng);

    // Convert to milliseconds and add some jitter (±10%)
    int64_t interval_ms = static_cast<int64_t>(interval_seconds * 1000.0);
    int64_t jitter = (static_cast<int64_t>(std::rand()) % 201) - 100; // ±100ms
    interval_ms += jitter;
    if (interval_ms < 10) {
        interval_ms = 10; // minimum 10ms
    }

    trigger_timer_ = reactor_.addTimer(static_cast<uint32_t>(interval_ms), [this]() {
        onTriggerFired();
    });
}

void Sensord::onTriggerFired() {
    if (!running_) {
        return;
    }

    uint64_t now_mono_ns = clock_.monotonic_ns();

    // Check debounce: skip if within debounce interval
    if (last_trigger_mono_ns_ > 0) {
        uint64_t elapsed_ms = (now_mono_ns - last_trigger_mono_ns_) / 1000000ULL;
        if (elapsed_ms < config_.debounce_ms) {
            triggers_suppressed_++;
            scheduleNextTrigger();
            return;
        }
    }

    // Send the trigger
    auto result = sendTrigger();
    if (!result.is_ok()) {
        std::cerr << "sensord: failed to send trigger: " << result.error().what() << "\n";
        // Still schedule next trigger (don't block the simulation)
        scheduleNextTrigger();
        return;
    }

    last_trigger_mono_ns_ = now_mono_ns;
    triggers_sent_++;
    std::cout << "sensord: trigger #" << triggers_sent_
              << " (suppressed: " << triggers_suppressed_ << ")\n";

    // Schedule next trigger
    scheduleNextTrigger();
}

Result Sensord::sendTrigger() {
    int peer_fd = bus_.connectedPeerFd();
    if (peer_fd < 0) {
        return Result::err(make_error(Error::Code::IoError, "sensord: not connected to eventd"));
    }

    flatbuffers::FlatBufferBuilder fbb(256);

    // Build Clock struct
    uint64_t mono_ns = clock_.monotonic_ns();
    int64_t wall_ms = clock_.wall_ms();
    camera::v1::Clock observed(mono_ns, wall_ms);

    // Build SensorTriggered table
    auto kind = SensorKind_Motion;
    SensorTriggeredBuilder st_builder(fbb);
    st_builder.add_sensor_id(config_.sensor_id);
    st_builder.add_kind(kind);
    st_builder.add_observed(&observed);
    st_builder.add_edge_count(1);
    auto st_flat = st_builder.Finish();
    fbb.Finish(st_flat);

    // Build Envelope
    Uuid device_uuid{};
    {
        std::string hex = config_.device_id_str;
        uint64_t hi = 0, lo = 0;
        for (int i = 0; i < 8; ++i) {
            hi = (hi << 4) | (hex[i] <= '9' ? hex[i] - '0' : hex[i] - 'a' + 10);
        }
        for (int i = 8; i < 16; ++i) {
            lo = (lo << 4) | (hex[i] <= '9' ? hex[i] - '0' : hex[i] - 'a' + 10);
        }
        device_uuid = Uuid{hi, lo};
    }

    auto source = ServiceId_Sensord;
    auto payload_type = Payload_SensorTriggered;
    auto priority = Priority_P1Normal;

    EnvelopeBuilder env_builder(fbb);
    env_builder.add_schema_version(1);
    env_builder.add_device_id(&device_uuid);
    env_builder.add_sent(&observed);
    env_builder.add_priority(priority);
    env_builder.add_source(source);
    env_builder.add_payload_type(payload_type);
    env_builder.add_payload(flatbuffers::Offset<void>(st_flat.Union()));
    auto envelope = env_builder.Finish();
    fbb.Finish(envelope);

    return bus_.sendReliable(peer_fd, fbb);
}

Result Sensord::connectToEventd() {
    auto result = bus_.connect(config_.eventd_path);
    if (!result.is_ok()) {
        std::cerr << "sensord: failed to connect to eventd at " << config_.eventd_path
                  << ": " << result.error().what() << "\n";

        // Retry after 2 seconds
        reactor_.addTimer(2000, [this]() {
            if (running_) {
                connectToEventd();
            }
        });

        return result;
    }

    return Result::ok();
}

void Sensord::onConnected() {
    connected_ = true;
    std::cout << "sensord: connected to eventd at " << config_.eventd_path << "\n";

    // Schedule first trigger
    scheduleNextTrigger();
}
