# Web Service

Dashboard with live updates using Server-Sent Events.

## Architecture

```mermaid
flowchart TD
    subgraph "Client Layer"
        Browser["Browser
        (SSE Client)"]
    end
    
    subgraph "Web Service" 
        WebService["web service
        (Go + Templates)"]
    end
    
    subgraph "Broker Layer"
        MQTT["MQTT Broker
        internal/stored/#"]
    end
    
    subgraph "Data Layer"
        Ingest["ingest service
        (MongoDB)"]
    end
    
    Browser -->|SSE connection | WebService
    WebService -->|MQTT publish | MQTT
    MQTT -->|MQTT subscribe | Browser
    MQTT -.->|Stores events | Ingest
    Ingest -.->|Commits events | MQTT
```

### Component Responsibilities

- **Browser**: SSE client with bounded buffer, resume support, keep-alive handling
- **Web Service**: SSE handler, event streaming, history replay, signed URL generation
- **MQTT Broker**: Message distribution, tenant isolation via topic partitioning
- **ingest**: Event storage, MongoDB operations, durability guarantees

## Endpoints

### Live Stream

```
GET /stream/events
```

Query parameters:

- `tenant_id` - Required (from session)
- `device_id` - Optional filter
- `type` - Optional filter

### History

```
GET /events
```

Calls the `api` service for history search.

## Server-Sent Events

### Subscription

Browser subscribes to `internal/stored/{tenant}` on the broker.

**Important**: This is a normal (non-shared) subscription, so every replica sees every notification.

### Notification Flow

```mermaid
sequenceDiagram
    participant DB as MongoDB
    participant Ingest as ingest service
    participant Broker as MQTT Broker
    participant Web as web service
    participant Browser as Browser (SSE)
    
    Note over DB,Browser: Event ingestion lifecycle
    
    DB->>Ingest: Commit event to MongoDB
    Ingest->>Broker: Publish to internal/stored/{tenant}
    Broker->>Web: Fan-out to subscribers
    Web->>Browser: Send SSE message
    Note over DB,Browser: UI updates with new event
    
    Note right of Browser: Bounded buffer (10K)
    Note right of Browser: Keep-alive every 15s
    Note right of Web: Resume from Last-Event-ID
    Note right of Broker: Topic-based tenant isolation
```

### SSE Messages

```mermaid
sequenceDiagram
    participant Browser as Browser
    participant Web as web service
    
    Note over Browser,Web: SSE Connection Setup
    Browser->>Web: GET /stream/events
    Web->>Browser: 200 OK
    
    Note over Browser,Web: Event Stream
    Web->>Browser: id: 1704067200,evt_001
    Web->>Browser: event: event
    Web->>Browser: data: {"type":"EventRaised",...}
    
    Note over Browser,Web: Keep-alive (15s)
    Web->>Browser: : keep-alive
    
    Note over Browser,Web: Resume (reconnect)
    Browser->>Web: Last-Event-ID: 1704067200,evt_001
    Web->>Browser: Replay missed events from MongoDB
    Web->>Browser: Resume live stream
```

### Resume Flow

```mermaid
flowchart LR
    subgraph "Browser"
        A["Slow connection / reconnect"]
        B["Send Last-Event-ID"]
    end
    subgraph "Web Service"
        C["Check Last-Event-ID"]
        D["Query MongoDB for gap"]
        E["Replay missed events"]
        F["Switch to live stream"]
    end
    subgraph "Data Layer"
        G["MongoDB"]
    end
    
    A --> B
    B --> C
    C -->|Valid ID | D
    C -->|Invalid/missing | H["Reload page"]
    D -->|Find gap | E
    E --> F
    D -.->|Fetch events | G
    E -.->|Replay events | G
    F -->|Resume streaming | Browser
    
    style H fill:#ffcccc
    style E fill:#ccffcc
```

### Slow Client Handling

- Bounded buffer per client
- Slow client disconnected
- Resume from cursor on reconnect

## Dashboard

### Features

- Live event stream
- Clip playback (short-lived signed URLs)
- Device status
- Fleet overview
- Error reports

### Clip Status

```mermaid
stateDiagram-v2
    [*] --> none
    none --> pending: Processing starts
    pending --> available: Clip ready
    pending --> failed: Processing failed
    available --> evicted: Memory pressure
    available --> [*]
    evicted --> [*]
    failed --> [*]
    
    note right of pending
        Short-lived signed URL
        Expiry: 60 seconds
    end note
    note right of available
        Playback URL generated
    end note
    note right of evicted
        Removed from spool
        No longer accessible
    end note
    
    classDef processing fill:#ffeb3b
    classDef success fill:#c8e6c9
    classDef error fill:#ffcdd2
    classDef none fill:#f5f5f5
    
    class pending processing
    class available success
    class failed error
    class evicted error
    class none none
```

| Status | Display |
|--------|--------|
| `none` | No clip |
| `pending` | "Clip processing" |
| `available` | Playable |
| `failed` | Failure state |
| `evicted` | Evicted from spool |

### History Pages

Call the `api` service for:

- Event search
- Device state
- Error reports

## Templates

Go templates with vanilla JavaScript:

- `dashboard.html` - Main dashboard
- `event.html` - Event detail
- `device.html` - Device detail
- `errors.html` - Error reports

## Observability

| Metric | Description |
|--------|-------------|
| `web_connections_total` | Active SSE connections (gauge) |
| `web_sse_messages_total` | SSE messages sent (counter) |
| `web_resume_requests_total` | Resume requests (counter) |
| `web_error_total` | Server errors (counter) |

## Health

- `/health` - Liveness probe
- `/ready` - Readiness probe
- `/metrics` - Prometheus metrics

## Security

- Tenant filtering on server
- Signed URLs for clip access (60s expiry)
- Session-based authentication

## Performance

| Metric | Target |
|--------|--------|
| Event-to-visible latency | < 5 seconds (p95) |
| SSE message latency | < 100ms |
| Buffer size | 10,000 messages |
| Keep-alive interval | 15 seconds |
