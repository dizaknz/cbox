#include "eventd/eventd.h"
#include "value/clock_helpers.h"
#include "value/uuid.h"

using namespace camera::v1;

namespace camera {

Eventd::Eventd(Reactor &reactor) : reactor_(reactor), bus_(reactor, ServiceId_Eventd), sim_source_(reactor) {}

Eventd::~Eventd() { stop(); }

Result Eventd::init(const EventdConfig &config) {
    config_ = config;

    // Parse UUIDs from hex strings
    auto parse_uuid = [](const std::string &hex) -> ResultT<UuidV7> {
        if (hex.size() != 32) {
            return ResultT<UuidV7>::err(make_error(Error::Code::InvalidArgument, "UUID hex must be 32 chars"));
        }
        UuidV7 id{};
        for (int i = 0; i < 16; ++i) {
            std::string byte_str = hex.substr(i * 2, 2);
            unsigned int byte_val;
            std::istringstream(byte_str) >> std::hex >> byte_val;
            if (i < 8) {
                id.hi = (id.hi << 8) | static_cast<uint64_t>(byte_val);
            } else {
                id.lo = (id.lo << 8) | static_cast<uint64_t>(byte_val);
            }
        }
        return ResultT<UuidV7>::ok(id);
    };

    auto dev_result = parse_uuid(config.device_id_str);
    if (!dev_result.ok())
        return Result::err(dev_result.error());
    device_uuid_ = std::move(dev_result.value());

    auto tenant_result = parse_uuid(config.tenant_id_str);
    if (!tenant_result.ok())
        return Result::err(tenant_result.error());
    tenant_uuid_ = std::move(tenant_result.value());

    boot_uuid_ = generate_uuid_v7();

    // Configure sim event source
    config_.sim_config.sensor_id = config.sensor_id;
    config_.sim_config.kind = static_cast<SensorKind>(config.sensor_kind);
    Result sim_err = sim_source_.configure(config_.sim_config);
    if (sim_err.is_err())
        return Result::err(sim_err.error());

    // Register bus handlers
    bus_.onPayload(Payload_SensorTriggered, [this](const Envelope *e, const uint8_t *d) { onSensorTriggered(e, d); });
    bus_.onPayload(Payload_CaptureNow, [this](const Envelope *e, const uint8_t *d) { onCaptureNow(e, d); });
    bus_.onPayload(Payload_ConfigUpdated, [this](const Envelope *e, const uint8_t *d) { onConfigUpdated(e, d); });

    // Register sim event callback
    sim_source_.onEvent([this](flatbuffers::FlatBufferBuilder &fbb) { onSimEvent(fbb); });

    return Result::ok();
}

void Eventd::start() {
    running_ = true;
    sim_source_.start();
}

void Eventd::stop() {
    running_ = false;
    sim_source_.stop();
    reactor_.stop();
}

void Eventd::run() { reactor_.run(); }

// ── Bus message handlers ─────────────────────────────────────────

void Eventd::onSensorTriggered(const Envelope *env, const uint8_t * /*data*/) {
    // Process incoming SensorTriggered from sensord (in sim mode, this comes from our own SimEventSource)
    auto triggered = env->payload_as_SensorTriggered();
    if (!triggered)
        return;

    SensorKind kind = static_cast<SensorKind>(triggered->kind());
    uint16_t sensor_id = triggered->sensor_id();
    (void)sensor_id;

    // Apply policy
    uint16_t suppressed = 0;
    if (should_suppress(kind)) {
        suppressed = 1;
        return; // Drop this event
    }

    record_event(kind);

    // Build and send EventRaised
    flatbuffers::FlatBufferBuilder fbb = buildRaisedEnvelope(triggered, kind, suppressed);
    bus_.publishNotify(fbb);
}

void Eventd::onCaptureNow(const Envelope *env, const uint8_t * /*data*/) {
    // Cloud-initiated capture command: create a Manual event immediately
    auto cmd = env->payload_as_CaptureNow();
    if (!cmd)
        return;

    // Check expiry
    int64_t now_ms = wall_ms();
    if (now_ms > static_cast<int64_t>(cmd->expires_wall_ms())) {
        return; // Command expired
    }

    // Build Manual event
    flatbuffers::FlatBufferBuilder fbb;
    auto occurred = make_clock(monotonic_ns(), now_ms);
    auto event_id = generate_uuid_v7();
    auto clip_id = generate_uuid_v7(); // placeholder — mediad will fill this

    EventRaisedBuilder raised(fbb);

    Uuid fb_event_id = to_fb_uuid(event_id);
    raised.add_event_id(&fb_event_id);
    raised.add_event_type(EventType_Manual);
    raised.add_occurred(&occurred);

    Uuid fb_clip_id = to_fb_uuid(clip_id);
    raised.add_clip_id(&fb_clip_id);
    raised.add_has_clip(false); // mediad hasn't created it yet
    raised.add_sensor_id(config_.sensor_id);
    raised.add_suppressed_count(0);

    raised.Finish();

    // Build Envelope wrapper
    EnvelopeBuilder env_builder(fbb);
    env_builder.add_schema_version(1);

    Uuid fb_msg_id = to_fb_uuid(generate_uuid_v7());
    env_builder.add_message_id(&fb_msg_id);

    Uuid fb_corr_id = to_fb_uuid(event_id);
    env_builder.add_correlation_id(&fb_corr_id);

    Uuid fb_dev_id = to_fb_uuid(device_uuid_);
    env_builder.add_device_id(&fb_dev_id);

    Uuid fb_tenant_id = to_fb_uuid(tenant_uuid_);
    env_builder.add_tenant_id(&fb_tenant_id);

    Uuid fb_boot_id = to_fb_uuid(boot_uuid_);
    env_builder.add_boot_id(&fb_boot_id);

    auto clock_synced = make_clock(monotonic_ns(), now_ms);
    env_builder.add_sent(&clock_synced);
    env_builder.add_clock_synced(true);
    env_builder.add_priority(Priority_P0Critical);
    env_builder.add_source(ServiceId_Eventd);
    env_builder.add_payload_type(Payload_EventRaised);
    auto payload = raised.Finish();
    env_builder.add_payload(payload.Union());
    auto envelope = env_builder.Finish();

    fbb.Finish(envelope);
    bus_.publishNotify(fbb);
}

void Eventd::onConfigUpdated(const Envelope *env, const uint8_t * /*data*/) {
    // Apply new configuration (cooldown, sensitivity, etc.)
    auto cfg = env->payload_as_ConfigUpdated();
    if (!cfg)
        return;

    auto settings = cfg->settings();
    if (settings) {
        config_.cooldown_s = settings->motion_cooldown_s();
    }
}

// ── Sim event handler ────────────────────────────────────────────

void Eventd::onSimEvent(flatbuffers::FlatBufferBuilder &envelope) {
    // Parse the SensorTriggered from the sim envelope
    const uint8_t *buf = envelope.GetBufferPointer();
    auto env = GetEnvelope(buf);
    if (!env || env->payload_type() != Payload_SensorTriggered)
        return;

    auto triggered = env->payload_as_SensorTriggered();
    if (!triggered)
        return;

    SensorKind kind = static_cast<SensorKind>(triggered->kind());
    uint16_t sensor_id = triggered->sensor_id();
    (void)sensor_id;

    // Apply policy
    uint16_t suppressed = 0;
    if (should_suppress(kind)) {
        suppressed = 1;
        return;
    }

    record_event(kind);

    // Build EventRaised
    flatbuffers::FlatBufferBuilder fbb = buildRaisedEnvelope(triggered, kind, suppressed);
    bus_.publishNotify(fbb);
}

// ── Policy engine ────────────────────────────────────────────────

bool Eventd::should_suppress(SensorKind kind) {
    (void)kind;
    uint64_t now_ns = monotonic_ns();
    auto key = config_.sensor_id; // Use sensor_id as the cooldown key

    auto it = cooldowns_.find(key);
    if (it == cooldowns_.end()) {
        return false; // First event, never suppress
    }

    CooldownState &state = it->second;

    uint64_t elapsed_ns = now_ns - state.last_event_ns;
    uint64_t cooldown_ns = static_cast<uint64_t>(config_.cooldown_s) * 1000000000ULL;

    if (elapsed_ns < cooldown_ns) {
        // Within cooldown window — increment suppression counter
        state.suppressed_count++;
        return state.suppressed_count >= static_cast<uint32_t>(config_.suppression_threshold);
    }

    // Cooldown expired — reset suppression counter but keep the event
    state.suppressed_count = 0;
    return false;
}

void Eventd::record_event(SensorKind kind) {
    (void)kind;
    uint64_t now_ns = monotonic_ns();
    auto key = config_.sensor_id;

    auto it = cooldowns_.find(key);
    if (it == cooldowns_.end()) {
        CooldownState state;
        state.last_event_ns = now_ns;
        state.suppressed_count = 0;
        cooldowns_[key] = std::move(state);
    } else {
        it->second.last_event_ns = now_ns;
        it->second.suppressed_count = 0;
    }
}

flatbuffers::FlatBufferBuilder
Eventd::buildRaisedEnvelope(const SensorTriggered *triggered, SensorKind kind, uint16_t suppressed_count) {
    flatbuffers::FlatBufferBuilder fbb;

    auto event_id = generate_uuid_v7();
    auto clip_id = generate_uuid_v7(); // placeholder for mediad
    int64_t now_ms = wall_ms();

    // Build EventRaised payload
    EventRaisedBuilder raised(fbb);

    Uuid fb_event_id = to_fb_uuid(event_id);
    raised.add_event_id(&fb_event_id);
    raised.add_event_type(static_cast<EventType>(kind));
    auto occurred = make_clock(monotonic_ns(), now_ms);
    raised.add_occurred(&occurred);

    Uuid fb_clip_id = to_fb_uuid(clip_id);
    raised.add_clip_id(&fb_clip_id);
    raised.add_has_clip(false); // Will be set by mediad
    raised.add_sensor_id(triggered->sensor_id());
    raised.add_suppressed_count(suppressed_count);

    auto payload = raised.Finish();

    // Build Envelope wrapper
    EnvelopeBuilder env(fbb);
    env.add_schema_version(1);

    Uuid fb_msg_id = to_fb_uuid(generate_uuid_v7());
    env.add_message_id(&fb_msg_id);

    Uuid fb_corr_id = to_fb_uuid(event_id);
    env.add_correlation_id(&fb_corr_id);

    Uuid fb_dev_id = to_fb_uuid(device_uuid_);
    env.add_device_id(&fb_dev_id);

    Uuid fb_tenant_id = to_fb_uuid(tenant_uuid_);
    env.add_tenant_id(&fb_tenant_id);

    Uuid fb_boot_id = to_fb_uuid(boot_uuid_);
    env.add_boot_id(&fb_boot_id);

    auto sent = make_clock(monotonic_ns(), now_ms);
    env.add_sent(&sent);
    env.add_clock_synced(true);
    env.add_priority(Priority_P1Normal);
    env.add_source(ServiceId_Eventd);
    env.add_payload_type(Payload_EventRaised);
    env.add_payload(payload.Union());
    auto envelope = env.Finish();

    fbb.Finish(envelope);
    return fbb;
}

} // namespace camera
