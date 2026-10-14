# Control Service

Handles device commands, configuration, and firmware rollouts.

## Responsibilities

1. **Commands**: CaptureNow, RebootDevice
2. **Configuration**: Desired state with versioning
3. **Rollouts**: Staged firmware deployment with automatic halt
4. **Manifests**: Register signed firmware release manifests

## Commands

### CaptureNow

```
POST /v1/devices/{id}/commands/capture
```

Body:
```json
{
  "pre_roll_ms": 3000,
  "post_roll_ms": 7000
}
```

Result: Publishes `CaptureNow` with message expiry.

### RebootDevice

```
POST /v1/devices/{id}/commands/reboot
```

Body:
```json
{
  "reason": "Configuration applied"
}
```

Result: Publishes `RebootDevice` with message expiry.

## Configuration

### Put Device Config

```
PUT /v1/devices/{id}/config
```

Body:
```json
{
  "motion_enabled": true,
  "motion_sensitivity": 50,
  "motion_cooldown_s": 30,
  "pre_roll_ms": 3000,
  "post_roll_ms": 7000,
  "clip_max_bytes": 10485760,
  "spool_quota_mb": 256,
  "upload_rate_cap_kbps": 0,
  "heartbeat_interval_s": 300,
  "telemetry_enabled": true
}
```

Result: Bumps `config_version`, stores settings, republishes `DesiredState`.

### Put Fleet Config

```
PUT /v1/fleets/{id}/config
```

Applies to all devices in the fleet.

## Rollouts

### Create Rollout

```
POST /v1/rollouts
```

Body:
```json
{
  "release_id": "...",
  "cohort": "all",
  "stages": [
    {"percent": 1, "soak_seconds": 3600},
    {"percent": 10, "soak_seconds": 3600},
    {"percent": 50, "soak_seconds": 7200},
    {"percent": 100, "soak_seconds": 0}
  ],
  "failure_threshold": 0.1
}
```

### Rollout Control

| Method | Endpoint |
|--------|----------|
| POST | `/v1/rollouts/{id}/pause` |
| POST | `/v1/rollouts/{id}/resume` |
| POST | `/v1/rollouts/{id}/abort` |

### Rollout Engine

1. Targets a cohort in stages (1%, 10%, 50%, 100%)
2. Composes each device's `DesiredState` from the database
3. Publishes as retained message to `fleet/+/+/status`
4. Watches `UpdateStatus` results (stored by `ingest`)
5. Halts automatically when failure/rollback rate exceeds threshold

### Automatic Halt

Triggers when:

- Failure rate > `failure_threshold`
- Rollback rate > `failure_threshold`
- Devices stop reporting status

## Manifest Registration

```
POST /v1/releases
```

Body:
```json
{
  "version": "1.2.3",
  "body": "...",
  "alg": "ed25519",
  "key_id": "...",
  "signature": "..."
}
```

**Important**: The `control` service never holds the firmware signing key. Release manifests are signed offline in the release pipeline and registered as-is. Devices verify them independently.

## Desired State

The `control` service is the **single writer** of device desired state, so configuration and firmware assignment never overwrite each other.

Desired state is retained on the broker:

```
fleet/+/+/status
```

Devices converge on reconnect to the retained message.

## Message Expiry

Commands have expiry timestamps:

- `CaptureNow`: 1 hour default
- `RebootDevice`: 30 minutes default

Expired commands are rejected with `CommandStatus: Expired`.

## Replay Protection

Per-command ID and replay window prevent command replay attacks.

## Observability

| Metric | Description |
|--------|-------------|
| `control_commands_total` | Commands issued (counter) |
| `control_config_applied_total` | Config applied (counter) |
| `control_rollouts_total` | Rollouts created (counter) |
| `control_rollouts_halted_total` | Rollouts halted (counter) |
| `control_command_duration` | Command processing latency (histogram) |

## Health

- `/health` - Liveness probe
- `/ready` - Readiness probe
- `/metrics` - Prometheus metrics
