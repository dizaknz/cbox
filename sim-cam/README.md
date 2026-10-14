# Camera Fleet Platform

Field camera devices (C++), cloud backend (Go), web dashboard, over-the-air (OTA) updates, and server-pushed commands.

## Overview

A fleet of camera devices in the field captures short video clips and raises events (motion detected, doorbell pressed). Each device uploads its events and clips to a cloud backend. A web dashboard shows events and clips as they arrive and lets operators browse history. Operators can also push commands and configuration to devices and roll out new firmware.

### Core Principles

1. **No event is silently lost** - Events are durable on the device until the cloud has acknowledged them.
2. **Everything is retryable and idempotent** - Delivery is at-least-once; duplicates are harmless.
3. **A device can always be recovered remotely** - Updates are atomic and roll back automatically.

## Project Structure

```
.
├── README.md                         # This file
├── ARCHITECTURE.md                    # Complete architecture specification
├── schemas/                           # FlatBuffers schemas (single source of truth)
├── device/                            # Device-side software
│   └── camera/
│       ├── libs/                      # Shared libraries
│       ├── services/                  # Device services
│       ├── adapters/                  # Hardware and simulation adapters
│       ├── packaging/                 # Build and deployment
│       └── tests/                     # Device tests
├── cloud/                             # Cloud backend
│   ├── ingest/                        # Event ingestion
│   ├── api/                           # REST API
│   ├── control/                       # Commands and rollouts
│   ├── web/                           # Dashboard
│   └── internal/                      # Shared Go packages
├── sim/                               # Simulation framework
├── tools/                             # Build and deployment tools
└── docs/                              # Documentation
```

## Quick Links

- [Schemas](schemas/README.md) - FlatBuffers message contracts
- [Device Services](device/camera/services/README.md) - C++ services for camera devices
- [Cloud Backend](cloud/README.md) - Go services for cloud operations
- [Simulation](sim/README.md) - Device simulation and fault injection
- [Tools](tools/README.md) - Schema tools and release signing

## Technology Stack

| Concern | Choice |
|---------|--------|
| Wire and IPC serialization | FlatBuffers (single schema for C++ and Go) |
| Device to cloud events and commands | MQTT 5 over TLS 1.3 with mutual authentication |
| Bulk transfer | HTTPS with pre-signed URLs |
| Local inter-process messaging | Unix domain sockets |
| Device supervision | systemd (socket activation, watchdog, cgroup limits) |
| Update framework (production) | RAUC with U-Boot A/B and boot counting |

## Fleet Scale

| Metric | Value |
|--------|-------|
| Devices | 100000 |
| Event rate | ~23 per second average, 20x peaks |
| Clip size | 2-10 MB (5 MB typical) |
| Clip volume | > TB per day |
| Heartbeat interval | 300 seconds |

## Getting Started

### Build

```bash
# C++ device services (host build with sanitizers)
cmake --preset=host-debug
cmake --build --preset=host-debug

# Cross-build for Yocto
cmake --preset=yocto
cmake --build --preset=yocto
```

### Run Simulation

```bash
docker compose up
```

See [Simulation](sim/) for scenario files and fault injection.

---

*For the complete architecture specification, see [ARCHITECTURE.md](docs/ARCHITECTURE.md).*
