# API Service

Read-only REST API for events, devices, and errors.

## Endpoints

### Events

| Method | Endpoint | Query Parameters |
|--------|----------|------------------|
| GET | `/v1/events` | `device_id`, `type`, `from`, `to`, `has_clip`, `limit`, `cursor` |
| GET | `/v1/events/{id}` | - |
| GET | `/v1/events/{id}/clip` | - |

**GET /v1/events** - Search with filters:

```
?device_id=uuid&type=motion&from=2024-01-01T00:00:00Z&to=2024-01-02T00:00:00Z&has_clip=true&limit=100&cursor=...
```

**GET /v1/events/{id}/clip** - Returns a short-lived signed GET URL (60 seconds).

### Devices

| Method | Endpoint |
|--------|----------|
| GET | `/v1/devices` |
| GET | `/v1/devices/{id}` |
| GET | `/v1/devices/{id}/errors` |

**GET /v1/devices** - Fleet state:

```json
{
  "devices": [
    {
      "device_id": "...",
      "tenant_id": "...",
      "status": "online",
      "last_seen": "2024-01-15T10:30:00Z",
      "firmware_version": "1.2.3",
      "reported_config_version": 5,
      "desired_config_version": 5,
      "update_state": "idle",
      "recent_errors": [...]
    }
  ]
}
```

**GET /v1/devices/{id}/errors** - Recent device error reports.

### Health

| Endpoint | Purpose |
|----------|---------|
| GET `/healthz` | Liveness probe |
| GET `/readyz` | Readiness probe |
| GET `/metrics` | Prometheus metrics |

## Authentication

Bearer token (OIDC JWT with tenant claim in production, static tokens in simulator).

Every query is filtered by tenant on the server.

## Pagination

Cursor-based, never offset-based:

```
GET /v1/events?limit=100&cursor=...
```

## Response Format

```json
{
  "events": [...],
  "cursor": "..."
}
```

## Error Handling

| Code | Meaning |
|------|---------|
| 401 | Unauthorized |
| 403 | Forbidden (tenant mismatch) |
| 404 | Not found |
| 410 | Resource deleted/archived |
| 429 | Rate limited |
| 500 | Server error |

## Observability

| Metric | Description |
|--------|-------------|
| `api_requests_total` | Total requests (counter) |
| `api_request_duration` | Request latency (histogram) |
| `api_errors_total` | Error responses (counter) |

## Security

- Tenant filtering on server
- Read-only access
- Signed URLs for clip access (60s expiry)
