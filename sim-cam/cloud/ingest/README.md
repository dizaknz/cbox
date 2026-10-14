# Ingest Service

Consumes MQTT events from devices, validates and stores them in MongoDB, and serves blob upload URLs.

## Responsibilities

1. **Subscribe** to MQTT topics for all devices
2. **Decode** inbound messages with hardened decoder
3. **Validate** identity and semantic rules
4. **Store** events idempotently in MongoDB
5. **Serve** pre-signed blob URLs for uploads
6. **Publish** internal notifications after commits

## Topics

### Subscriptions

| Topic | Purpose | QoS |
|-------|---------|-----|
| `$share/ingest/fleet/+/+/up/#` | Upload events (clips, status) | 1 (shared) |
| `fleet/+/+/status` | Device status and heartbeats | 1 (shared) |

### Publishing

| Topic | Purpose | Trigger |
|-------|---------|---------|
| `internal/stored/{tenant}` | Fan-out to web service | After MongoDB commit |
| `internal/dead-letter` | Invalid messages | Decoder/validator failure |

## Idempotency

All writes are **idempotent upserts**, keyed by:

- Event messages: `message_id` or `event_id`
- Upload messages: `clip_id` or `message_id`

Redelivery from QoS 1 is harmless.

## Out-of-Order Handling

A `ClipUploaded` arriving before its `EventRaised`:

1. Create event document with `$setOnInsert` defaults
2. Insert clip sub-document with `status: "pending"`
3. Later `EventRaised` fills in the rest

## Blob URL Requests

Devices request upload URLs via RPC on `$share/ingest/fleet/+/+/up/rpc`:

| Request | Response |
|---------|----------|
| `BlobUrlRequest` | `BlobUrlGranted` with pre-signed PUT URL |

Authorization checks:

- Device not revoked
- Tenant quota available
- Size limit respected
- Clip or release is known

## Dead Letter

Invalid or undecodable messages:

1. Go runtime wraps decoding in size-limited, panic-recovering decoder
2. Semantic validation failures go to dead-letter topic
3. Capped `dead_letters` collection
4. Poison message never blocks the consumer

## MongoDB Collections

| Collection | Purpose | TTL |
|------------|---------|-----|
| `events` | Event documents (with clip sub-doc) | 90 days |
| `devices` | Device state, heartbeats, update status | - |
| `device_errors` | Error reports from devices | - |
| `dead_letters` | Invalid messages (capped) | - |

## Metrics

| Metric | Description |
|--------|-------------|
| `ingest_messages_total` | Inbound messages (counter) |
| `ingest_decoding_errors_total` | Decoder failures (counter) |
| `ingest_storage_errors_total` | MongoDB errors (counter) |
| `ingest_blob_requests_total` | Blob URL requests (counter) |
| `ingest_consumer_lag` | Consumer lag (gauge) |
| `ingest_storage_duration` | Storage latency (histogram) |

## Health

- `/health` - Liveness probe
- `/ready` - Readiness probe (broker connected, MongoDB connected)
- `/metrics` - Prometheus metrics

## Security

- Mutual TLS with broker
- ACLs scoped to own topics
- Tenant identity validated against topic
- Blob URLs are pre-signed with size and checksum headers
