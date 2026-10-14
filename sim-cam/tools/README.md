# Tools

Build and deployment tools.

## Schema Tools

### schema-dump

Dumps any FlatBuffer as JSON using the schemas.

```bash
./schema-dump < buffer.bin > output.json
```

Usage:

```
schema-dump [-s SCHEMA] < file.bin
```

- `-s SCHEMA`: Schema name (default: `Envelope`)
- Outputs JSON with payload type and IDs, not raw bytes

### release-sign

Signs firmware release manifests.

```bash
release-sign --version 1.2.3 --image firmware.tar --key release.key --out manifest.fbs
```

Usage:

```
release-sign --version VERSION --image IMAGE --key KEY --out OUTPUT
```

- Signs manifest with Ed25519
- Produces nested FlatBuffer with signature

### manifest-build

Builds update manifests from images.

```bash
manifest-build --release 1.2.3 --image firmware.tar --out manifest.fbs
```

Usage:

```
manifest-build --release RELEASE --image IMAGE --out OUTPUT
```

## Deployment Tools

### deploy/broker-acl

Generates EMQX ACL configuration for the fleet.

```bash
deploy/broker-acl --tenant TENANT_ID --output acl.json
```

### deploy/docker-compose

One-command start with scenario file:

```bash
docker compose up
```

Configurable:

- Number of devices
- Scenario file
- Network settings

## Build Tools

### device/camera/Makefile

Builds device services:

```bash
make build          # Host build
make cross          # Cross-build for Yocto
make test           # Run tests
```

### cloud/Makefile

Builds cloud services:

```bash
make build          # Build all services
make test           # Run tests
make docker         # Build Docker images
```

## CI Tools

### .github/workflows/build.yml

CI pipeline:

1. Build matrix (host with sanitizers, cross)
2. Static analysis
3. Unit and fuzz tests
4. Schema compatibility check
5. Docker image builds
6. Integration tests with simulated devices

### .github/workflows/integration.yml

Integration tests:

1. Start compose environment
2. Run fault-injection scenarios
3. Assert no loss, no duplicates, bounded latency

## Release Tools

### release/

Release artifacts:

```
release/
├── device/           # Device binaries
├── cloud/            # Cloud binaries
├── docker/           # Docker images
└── manifest/         # Release manifests
```

### release/verify

Verifies release artifacts:

```bash
release/verify --device VERSION --cloud VERSION --manifest MANIFEST
```

## Documentation

All tools have man pages:

```bash
./tool --help
man tool
```
