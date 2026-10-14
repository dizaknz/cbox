# Simulation Framework

Simulates camera devices for testing and demonstration.

## Architecture

```
┌─────────────────────────────────────┐
│      Device Supervisor              │
│      (docker-compose.yml)           │
└─────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────┐
│         Simulated Device            │
│  (real binaries + sim adapters)     │
└─────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────┐
│    Simulation Adapters              │
│  • SimEventSource                   │
│  • SyntheticClipSource              │
│  • SimClock                         │
│  • SimSlotBackend                   │
└─────────────────────────────────────┘
```

## Components

### Device Supervisor

Manages simulated devices:

- Starts real service binaries
- Injects simulation adapters at build time
- Runs many devices on one machine
- Handles lifecycle (start/stop/restart)

### Simulation Adapters

Real binaries link hardware adapters; simulator binaries link simulation adapters:

| Adapter | Production | Simulation |
|---------|------------|------------|
| Event source | GPIO hardware | `SimEventSource` |
| Clip source | Camera pipeline | `SyntheticClipSource` |
| Clock | System clock | `SimClock` (injectable) |
| Slot backend | RAUC/U-Boot | `SimSlotBackend` |

**No simulation code ships in the field image.**

### Scenarios

Declarative files describe per-device behavior:

```yaml
# scenario.yml
devices:
  - device_id: "device-001"
    tenant_id: "tenant-001"
    event_source:
      type: "motion"
      rate_per_hour: 20
      burst_factor: 1.5
    clip_source:
      type: "synthetic"
      duration_ms: 5000
      codec: "h264"
      quality: "medium"
    clock_offset_ms: 0
  - device_id: "device-002"
    tenant_id: "tenant-001"
    event_source:
      type: "doorbell"
      rate_per_hour: 5
    clip_source:
      type: "synthetic"
      duration_ms: 3000
```

### Clock

Injectable clock for scenario timelines:

- Run faster than real time
- Precise backoff timer control
- Deterministic behavior

### Slot Backend

Simulates A/B update slots:

- `slotA/` and `slotB/` directories
- Boot-control state file
- "Reboot" restarts process group from selected slot
- Firmware images are tarballs with Ed25519-signed manifest

## Fault Injection

Control tool and network proxy:

| Injection | Tool |
|-----------|------|
| Drop network | Network proxy |
| Add latency | Network proxy |
| Throttle bandwidth | Network proxy |
| Return 5xx/403 | Network proxy |
| Truncate/corrupt clip | Network proxy |
| Kill service | Control tool |
| Fill spool | Control tool |
| Skew device clock | Control tool |

## Demonstration Scenarios

### 1. Steady State

N devices running normally, events and clips flowing.

### 2. Broker Outage

- Devices buffer in outbox
- No lost or duplicated events
- Drain with jittered backoff on reconnect

### 3. Object Store Failure

- Retries with backoff
- `UploadFailed` event reported
- Dashboard shows failed clip

### 4. Staged Rollout (Good Release)

- 1% → 10% → 50% → 100%
- Automatic progression
- Success metrics

### 5. Staged Rollout (Bad Release)

- Health check fails
- Automatic rollback
- Rollout halts when failure rate exceeds threshold

## Running

```bash
docker compose up
```

Configurable:

- Number of devices
- Scenario file
- Fault injection

## Isolation

Each device has its own:

- Directory tree (`outbox/`, `spool/`, `state/`, `config/`, `update/`)
- Sockets
- Certificates (local CA)

## Scalability

Simulator runs many devices on one machine:

- Shared broker (EMQX)
- Shared MongoDB
- Shared object store (MinIO)

## Tools

| Tool | Purpose |
|------|---------|
| `sim/scenarios/generate` | Generate scenario files |
| `sim/tools/inject-fault` | Fault injection control |
| `sim/tools/status` | Fleet status |
| `sim/tools/rollback` | Force device rollback |

## Metrics

| Metric | Description |
|--------|-------------|
| `sim_devices_total` | Simulated devices (gauge) |
| `sim_events_generated_total` | Events generated (counter) |
| `sim_clips_uploaded_total` | Clips uploaded (counter) |
| `sim_fault_injections_total` | Fault injections (counter) |
