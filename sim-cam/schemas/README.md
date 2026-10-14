# FlatBuffers Schemas

The schemas directory contains the single source of truth for all message contracts used across the platform.

## Purpose

- Define all messages exchanged between device services and the cloud backend
- Enable zero-copy deserialization in C++ and Go
- Support schema evolution with backward compatibility

## Organization

All schemas live in this directory and are included in the build:

```
schemas/
├── common.fbs           # Shared types (Uuid, Clock, ClockSkew, etc.)
├── bus.fbs              # Bus and messaging primitives
├── events.fbs           # Event and clip definitions
├── media.fbs            # Media handling (clips, thumbnails)
├── link.fbs             # Link state and device status
├── control.fbs          # Configuration and commands
├── ota.fbs              # Firmware update messages
└── envelope.fbs         # Top-level message wrapper
```

## Schema Rules

| Rule | Detail |
|------|--------|
| **Evolution** | Add fields only at the end of a table; add union members only at the end; never reorder, remove, or change types |
| **Enums** | Every enum has `Unknown = 0`; receivers treat unknown values as `Unknown` |
| **Verification** | The C++ `Verifier` runs on every inbound buffer; Go validates semantically |
| **Semantic validation** | Lengths, ranges, identity checks, non-empty required fields |
| **Signed content** | `SignedManifest.body` is a nested FlatBuffer with signature covering exact bytes |
| **Alignment** | Receive buffers are 8-byte aligned for zero-copy reads |

## Building Schema Code

### C++ Device Build

```bash
flatc --cpp --grpc schemas/*.fbs -o device/camera/libs/schema/
```

### Go Cloud Build

```bash
flatc --go schemas/*.fbs -o cloud/internal/generated/
```

## Schema Evolution

Run CI checks before committing schema changes:

```bash
# Check compatibility with last released schema
flatc --conform old_schema_set.fbs new_schema_set.fbs
```

CI runs `flatc --conform` against the last released schema set and fails on incompatible changes.

## Tools

- **Schema JSON dump**: `tools/schema-dump` - Dumps any buffer as JSON using the schemas
- **Release signing**: `tools/release-sign` - Signs firmware release manifests
- **Manifest builder**: `tools/manifest-build` - Builds update manifests from images

## Wire Format

Messages on the bus and the wire are identical `Envelope` buffers. Domain logic converts them to plain value types at service boundaries.

See [ARCHITECTURE.md](../docs/ARCHITECTURE.md#5-messaging-contracts) for complete schema definitions.
