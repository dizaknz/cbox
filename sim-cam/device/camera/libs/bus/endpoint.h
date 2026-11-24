#pragma once
// endpoint.h — SOCK_SEQPACKET Unix socket bus endpoint
// Handles connection management, Hello/Subscribe handshake,
// reliable (acknowledged) and notify delivery classes.

#include "generated/bus_generated.h"
#include "generated/common_generated.h"
#include "generated/envelope_generated.h"
#include "reactor/reactor.h"
#include "value/result.h"
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace camera::v1;

namespace camera {

// ── Reliable message tracking ────────────────────────────────────

struct PendingAck {
    flatbuffers::FlatBufferBuilder builder; // serialized Envelope
    uint64_t first_attempt_ns;
    int attempt_count;
};

// ── BusEndpoint ──────────────────────────────────────────────────

class BusEndpoint {
  public:
    BusEndpoint(Reactor &reactor, ServiceId service_id);
    ~BusEndpoint();

    // Listen on a Unix socket path. Returns Error if binding fails.
    Result listen(const std::string &path);

    // Connect to an existing Unix socket. Returns Error if connection fails.
    Result connect(const std::string &path);

    // Set the payload type handler. Called when a message with that payload type arrives.
    using PayloadHandler = std::function<void(const Envelope *env, const uint8_t *data)>;
    void onPayload(Payload payload_type, PayloadHandler handler);

    // Send a reliable (acknowledged) message to a connected peer.
    // The sender will retry with exponential backoff until acknowledged.
    Result sendReliable(int peer_fd, flatbuffers::FlatBufferBuilder &envelope);

    // Publish a notify (best-effort fan-out) message to all connected peers.
    void publishNotify(flatbuffers::FlatBufferBuilder &envelope);

    // Get the listening socket fd (for reactor registration).
    int listenFd() const { return listen_fd_; }

    // Check if endpoint is listening.
    bool isListening() const { return listen_fd_ >= 0; }

    // Set a callback invoked when a client connection is established.
    using ConnectionCallback = std::function<void(int peer_fd)>;
    void onConnected(ConnectionCallback cb);

    // Get the first connected peer fd, or -1 if none.
    int connectedPeerFd() const;

  private:
    static constexpr size_t MAX_MESSAGE_SIZE = 65536; // 64 KiB
    static constexpr uint64_t RELIABLE_RETRY_BASE_MS = 100;
    static constexpr uint64_t RELIABLE_RETRY_MAX_MS = 5000;
    static constexpr uint64_t RELIABLE_DEADLINE_S = 30;

    // ── Connection state ────────────────────────────────────────
    struct PeerConnection {
        int fd;
        ServiceId peer_service;
        bool hello_sent = false;
        bool subscribed = false;
        bool connected_handled = false;
        std::string path;

        // Reliable send buffer: messages waiting for ack
        std::deque<PendingAck> reliable_pending;

        // Receive buffer (partial message)
        uint8_t recv_buf[MAX_MESSAGE_SIZE];
        size_t recv_pos = 0;
    };

    void onListenReady();
    void onPeerReadable(int peer_fd);
    void onPeerWritable(int peer_fd);
    void handleHello(int peer_fd, const Hello *hello);
    void handleSubscribe(int peer_fd, const Subscribe *sub);
    bool handleMessage(int peer_fd, const uint8_t *data, size_t len);
    void dispatchMessage(const Envelope *env, const uint8_t *data);

    // Reliable send retry timer callback
    void onReliableRetryTimer();

    // Schedule reliable retry timer
    void scheduleReliableRetry();
    void cancelReliableRetry();

    // Flush pending acks for a peer
    void flushPending(int peer_fd);

    // Read exactly n bytes from a fd (returns Error on failure)
    Result readExact(int fd, uint8_t *buf, size_t len);

    Reactor &reactor_;
    ServiceId service_id_;
    int listen_fd_ = -1;
    std::unordered_map<int, PeerConnection> peers_; // fd -> connection state
    std::unordered_map<Payload, PayloadHandler> handlers_;

    TimerId reliable_retry_timer_ = 0;
    bool retry_scheduled_ = false;

    ConnectionCallback connection_cb_;
};

} // namespace camera
