# Agent prompts

## Setup

* [llama.cpp](https://llama.app/)
* [pi](https://pi.dev/)
* [Qwen 3.5 MTP](https://huggingface.co/unsloth/Qwen3.5-9B-MTP-GGUF)

## Ideation

### High level architecture

A monorepo that contains:
- simulated camera device/firmware written in modern C++ 20
  - motion (PIR) or doorbell (button/GPIO) trigger event to record video/audio clip
  - finalise clip and upload media to cloud blob storage
  - emit event to external message broker (topic) with trigger data and link to storage
- ingest microservice written in golang (consumer)
  - consumes messages of the message topic for devices/tenant 
  - writes all event meta data to a NoSQL DB including storage link 
  - data is immutable and have TTL, default to 10 days
- API service written in golang
  - retrieving media records and data from NoSQL DB and blob storage
  - support searching historical data
- web app written in golang/HTML
  - uses Server Send Events for real time data 
  - support querying API for historical data

```
 Simulated devices (C++ firmware, N instances)
   |  events: MQTT over mTLS (QoS 1)        |  clips: HTTPS PUT via pre-signed URL
   v                                        v
 MQTT broker -----------------+        Object storage (MinIO / S3)
   |  shared subscription     |                  ^
   v                          |                  | signed GET URLs
 Ingest service (Go) --> NoSQL DB <-- Query API (Go) <-- Web (Go + HTML, SSE)
   |                                                        ^
   +---- "event stored" internal topic ---------------------+
```

### Low level architecture

Let's just focus on the simulated camera device
- target a device with constrained memory
- use modern C++20, `noexecpt`, value semantics, compile time dispatch, CRTP and `concept`
- split into multiple cooperating services, not one process
- simulated data, clips and events are generated, not recorded

Implement as an event driven architecture
- use message passing not shared state for communicating between processes for the trigger events
- consider a distributed reactor pattern binding to hardware events for motion detection or a doorbell press
- once an event is triggered it needs to be processed, the media uploaded to blob storage with backoff/retry build in
- use internal messaging system for interprocess communication, prefer flatbuffers
- failure and error events are send to local service to emit to external messaging system
- consider MQTT with flatbuffer for external messaging system

```
  GPIO / PIR / button (sim: scenario script)
            |
        [ sensord ]  --SensorTriggered-->  [ eventd ]  --RecordClip-->  [ mediad ]
                                              |   ^                         |
                                  EventRaised |   | ClipReady / failures    | files in spool/ (ownership
                                              v   |                         v  handed over by message)
                                          [ linkd ] <--UploadUrl/results-- [ uploadd ] --HTTPS PUT--> blob storage
                                              |
                                  MQTT 5 over mTLS (events, errors, telemetry, config)
                                              v
                                         cloud broker
```

Manually create an `ARCHITECTURE.md` and iterate on it with `pi`.

## Agent prompts

- `@ARCHITECTURE.md`

```
 This is a comprehensive architecture document for a simulated camera device and media event system. Let me analyze the key components:

 1. Overall System: A distributed IoT camera system with embedded devices that capture video clips and raise events (motion detection, doorbell press)

 2. Key Components:
     - Simulated devices running C++ firmware
     - MQTT broker for messaging (mTLS with QoS 1)
     - Object storage (MinIO/S3) for media clips
     - Ingest service (Go) to process events and store data
     - NoSQL database for persistent storage of immutable data with TTL
     - Query API (Go) for retrieving historical data
     - Web interface (Go/HTML) using Server-Sent Events

 3. Device Architecture:
     - Multiple systemd services running on embedded devices:
         - sensord: hardware input handling
         - eventd: event coordination
         - mediad: camera/encoder
         - uploadd: upload service
         - linkd: MQTT connection

 4. Communication:
     - Device events via MQTT (QoS 1) over mTLS
     - Media clips uploaded via HTTPS PUT to pre-signed URLs
     - Internal communication between services using Unix domain sockets with FlatBuffers

 5. Data Flow:
     - Devices publish events to MQTT
     - Ingest service consumes events, stores metadata and media
     - Ingested events are published to internal topic
     - Live stream service broadcasts via SSE to web clients
 Would you like me to help you with any specific aspect of this architecture? For example, I could help you implement one of the services, create documentation for a component, or explain how a particular part works.
```

- Start by writing an `ARCHITECTURE.md` for the embedded device under `device`. Note this is a cmake monorepo for modern C++20
- First create a minimal CMake file for building on a Linux target
- ...




