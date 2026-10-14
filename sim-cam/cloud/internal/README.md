# Internal Shared Packages

Shared Go packages and generated schema code used across cloud services.

## Structure

```
cloud/internal/
├── generated/          # FlatBuffers generated code
├── decoder/            # Hardened message decoder
├── models/             # Domain models
├── auth/               # Authentication and authorization
└── metrics/            # Shared metrics
```

## Generated Code

FlatBuffers schemas are generated here for use in all cloud services:

```go
// Generated from schemas/*.fbs
package schema

type Envelope struct { ... }
type Payload struct { ... }
type EventRaised struct { ... }
// ... all message types
```

Generated via:

```bash
flatc --go schemas/*.fbs -o cloud/internal/generated/
```

## Hardened Decoder

The decoder is designed to survive malicious or corrupted input:

```go
// decoder/decoder.go
func Decode(data []byte) (*Envelope, error) {
    // Size-limited decoding
    // Panic recovery
    // Semantic validation
}
```

### Panic Recovery

```go
defer func() {
    if r := recover(); r != nil {
        // Log and dead-letter
    }
}()
```

### Semantic Validation

- Length checks (`sha256` is exactly 32 bytes)
- Range validation
- Identity checks (`device_id` and `tenant_id` match authenticated session)
- Non-empty required fields
- Bounded vector lengths

## Domain Models

Plain value types for business logic:

```go
// models/event.go
type Event struct {
    ID         string    `json:"id"`
    TenantID   string    `json:"tenant_id"`
    DeviceID   string    `json:"device_id"`
    Type       string    `json:"type"`
    OccurredAt time.Time `json:"occurred_at"`
    Attributes json.RawMessage `json:"attributes"`
    Clip       *Clip     `json:"clip,omitempty"`
}

type Clip struct {
    ID         string    `json:"id"`
    Status     string    `json:"status"` // none, pending, available, failed, evicted
    ObjectKey  string    `json:"object_key"`
    SizeBytes  int64     `json:"size_bytes"`
    SHA256     [32]byte  `json:"sha256"`
    UploadedAt time.Time `json:"uploaded_at"`
}
```

## Authentication

### Tenant Filtering

Every query is filtered by tenant on the server:

```go
func FilterByTenant(ctx context.Context, query mongo.Filter) {
    tenant := auth.TenantFromContext(ctx)
    query["tenant_id"] = tenant.ID
}
```

### Auth Context

```go
type AuthContext struct {
    Tenant *Tenant
    UserID string
    Roles  []string
}
```

## Metrics

Shared metrics registry:

```go
// metrics/registry.go
var (
    MessagesReceived = prometheus.NewCounterVec(
        prometheus.CounterOpts{
            Name: "cloud_messages_received_total",
            Help: "Total messages received",
        },
        []string{"service", "topic"},
    )
)
```

## Blob URL Generation

Pre-signed URLs for clips and firmware:

```go
// blob/blob.go
func GenerateUploadURL(ctx context.Context, clipID string) (string, error) {
    // Check tenant quota
    // Check size limit
    // Generate pre-signed PUT URL
}

func GenerateDownloadURL(ctx context.Context, objectKey string) (string, error) {
    // Generate pre-signed GET URL (60s expiry)
}
```

## Error Types

```go
// errors/errors.go
type DecodeError struct {
    Message string
    Offset  int
}

type ValidationError struct {
    Field  string
    Reason string
}

type AuthError struct {
    Code    string
    Message string
}
```

## Build

```bash
cd cloud/internal
go generate
go build ./...
```
