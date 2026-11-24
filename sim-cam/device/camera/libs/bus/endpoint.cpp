#include "bus/endpoint.h"

#ifdef _WIN32
// clang-format off
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
// clang-format on
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace camera {

// ── Cross-platform socket helpers ────────────────────────────────

#ifdef _WIN32
using socklen_t_int = int;
static inline int win_send(int fd, const void *buf, size_t len) {
    return send(fd, static_cast<const char *>(buf), static_cast<int>(len), 0);
}
static inline int win_recv(int fd, void *buf, size_t len) {
    return recv(fd, static_cast<char *>(buf), static_cast<int>(len), 0);
}
static inline bool win_is_wouldblock(int err) { return err == WSAEWOULDBLOCK; }
#else
static inline int win_send(int fd, const void *buf, size_t len) { return write(fd, buf, len); }
static inline int win_recv(int fd, void *buf, size_t len) { return read(fd, buf, len); }
static inline bool win_is_wouldblock(int err) { return (err == EAGAIN || err == EWOULDBLOCK); }
#endif

BusEndpoint::BusEndpoint(Reactor &reactor, ServiceId service_id) : reactor_(reactor), service_id_(service_id) {}

BusEndpoint::~BusEndpoint() {
    cancelReliableRetry();
    if (listen_fd_ >= 0) {
#ifdef _WIN32
        closesocket(listen_fd_);
#else
        close(listen_fd_);
#endif
        listen_fd_ = -1;
    }
    for (auto &[fd, peer] : peers_) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
    }
}

#ifndef _WIN32

Result BusEndpoint::listen(const std::string &path) {
    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return Result::err(make_error(Error::Code::IoError, "socket() failed: %s", strerror(errno)));

    // Remove stale socket file
    unlink(path.c_str());

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return Result::err(make_error(Error::Code::IoError, "bind() failed: %s", strerror(errno)));
    }

    if (listen(fd, 16) < 0) {
        close(fd);
        return Result::err(make_error(Error::Code::IoError, "listen() failed: %s", strerror(errno)));
    }

    // Set non-blocking
    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    listen_fd_ = fd;

    reactor_.addFd(fd, EventFlags::Read, [this](EventFlags) { onListenReady(); });

    return Result::ok();
}

Result BusEndpoint::connect(const std::string &path) {
    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return Result::err(make_error(Error::Code::IoError, "socket() failed: %s", strerror(errno)));

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    // Set non-blocking for connect
    int flags = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    int ret = connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        close(fd);
        return Result::err(make_error(Error::Code::IoError, "connect() failed: %s", strerror(errno)));
    }

    // Register for write readiness (connect completion)
    auto conn = std::make_unique<PeerConnection>();
    conn->fd = fd;
    conn->path = path;

    int peer_fd = fd;
    peers_[peer_fd] = std::move(*conn);
    std::unique_ptr<PeerConnection> conn_ptr = nullptr; // ownership transferred

    reactor_.addFd(fd, EventFlags::Write, [this, peer_fd](EventFlags) { onPeerWritable(peer_fd); });

    return Result::ok();
}

void BusEndpoint::onListenReady() {
    struct sockaddr_un client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(listen_fd_, reinterpret_cast<struct sockaddr *>(&client_addr), &addr_len);

    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Accept error — ignore for now
        }
        return;
    }

    fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL) | O_NONBLOCK);

    auto conn = std::make_unique<PeerConnection>();
    conn->fd = client_fd;
    conn->path = std::string(client_addr.sun_path, strlen(client_addr.sun_path));

    int fd = client_fd;
    peers_[fd] = std::move(*conn);

    // Register for read
    reactor_.addFd(client_fd, EventFlags::Read, [this, fd](EventFlags) { onPeerReadable(fd); });
}

void BusEndpoint::onPeerReadable(int peer_fd) {
    auto it = peers_.find(peer_fd);
    if (it == peers_.end())
        return;

    PeerConnection &peer = it->second;

    // Read all available data
    while (true) {
        int n = win_recv(peer.fd, peer.recv_buf + peer.recv_pos, MAX_MESSAGE_SIZE - peer.recv_pos);

        if (n < 0) {
            if (win_is_wouldblock(errno))
                break;
            // Connection error — close peer
            peers_.erase(it);
            close(peer_fd);
            return;
        }

        if (n == 0) {
            // Connection closed
            peers_.erase(it);
            close(peer_fd);
            return;
        }

        peer.recv_pos += static_cast<size_t>(n);

        // Process complete messages
        while (peer.recv_pos >= 4) {
            // First 4 bytes: message length (uint32, big-endian)
            uint32_t msg_len;
            if (peer.recv_pos < 4)
                break;
            memcpy(&msg_len, peer.recv_buf, 4);
            msg_len = ntohl(msg_len);

            if (msg_len == 0 || msg_len > MAX_MESSAGE_SIZE) {
                // Invalid message length — reset
                peer.recv_pos = 0;
                break;
            }

            if (peer.recv_pos < 4 + msg_len)
                break; // incomplete message

            const uint8_t *msg_data = peer.recv_buf + 4;
            bool ok = handleMessage(peer.fd, msg_data, static_cast<size_t>(msg_len));
            if (!ok) {
                peers_.erase(it);
                close(peer_fd);
                return;
            }

            // Advance buffer
            peer.recv_pos -= (4 + msg_len);
            if (peer.recv_pos > 0) {
                memmove(peer.recv_buf, peer.recv_buf + 4 + msg_len, peer.recv_pos);
            }
        }
    }
}

void BusEndpoint::onPeerWritable(int peer_fd) {
    auto it = peers_.find(peer_fd);
    if (it == peers_.end())
        return;

    PeerConnection &peer = it->second;

    // Send Hello
    if (!peer.hello_sent) {
        flatbuffers::FlatBufferBuilder fbb;
        camera::v1::HelloBuilder hb(fbb);
        hb.service(static_cast<camera::v1::ServiceId>(service_id_));
        hb.schema_version(1);
        hb.pid(static_cast<uint32_t>(getpid()));
        Hello *hello = hb.Finish();
        fbb.Finish(hello);

        uint32_t len = static_cast<uint32_t>(fbb.GetSize());
        uint32_t net_len = htonl(len);
        win_send(peer.fd, &net_len, 4);
        win_send(peer.fd, fbb.GetBufferPointer(), len);

        peer.hello_sent = true;

        // Now send Subscribe
        flatbuffers::FlatBufferBuilder sub_fbb;
        std::vector<uint8_t> types;
        types.push_back(static_cast<uint8_t>(Payload_Ack));
        types.push_back(static_cast<uint8_t>(Payload_BlobUrlGranted));
        types.push_back(static_cast<uint8_t>(Payload_BlobUrlDenied));
        types.push_back(static_cast<uint8_t>(Payload_ConfigUpdated));
        types.push_back(static_cast<uint8_t>(Payload_CaptureNow));
        types.push_back(static_cast<uint8_t>(Payload_RebootDevice));
        types.push_back(static_cast<uint8_t>(Payload_UpdateManifestReceived));
        types.push_back(static_cast<uint8_t>(Payload_QuiesceRequest));

        camera::v1::SubscribeBuilder sb(sub_fbb);
        sb.payload_types(types);
        sb.replay_state(true);
        Subscribe *sub = sb.Finish();
        sub_fbb.Finish(sub);

        uint32_t sub_len = static_cast<uint32_t>(sub_fbb.GetSize());
        uint32_t sub_net = htonl(sub_len);
        win_send(peer.fd, &sub_net, 4);
        win_send(peer.fd, sub_fbb.GetBufferPointer(), sub_len);

        peer.subscribed = true;
    }

    // Flush pending reliable messages
    cancelReliableRetry();
    flushPending(peer_fd);
    scheduleReliableRetry();

    // No longer need write event — switch to read-only
    reactor_.removeFd(peer_fd);
    reactor_.addFd(peer_fd, EventFlags::Read, [this, peer_fd](EventFlags) { onPeerReadable(peer_fd); });

    // Invoke connection callback (only once, when Hello/Subscribe complete)
    if (peer.connected_handled) {
        return;
    }
    peer.connected_handled = true;
    if (connection_cb_) {
        connection_cb_(peer_fd);
    }
}

#else

// ── Windows stub implementations ────────────────────────────────

Result BusEndpoint::listen(const std::string &path) {
    // TODO: Windows Unix socket support via WSA
    (void)path;
    return Result::err(make_error(Error::Code::Internal, "Unix sockets not fully supported on Windows"));
}

Result BusEndpoint::connect(const std::string &path) {
    (void)path;
    return Result::err(make_error(Error::Code::Internal, "Unix sockets not fully supported on Windows"));
}

void BusEndpoint::onListenReady() {}
void BusEndpoint::onPeerReadable(int peer_fd) { (void)peer_fd; }
void BusEndpoint::onPeerWritable(int peer_fd) { (void)peer_fd; }

#endif // _WIN32

// ── Shared message handling ──────────────────────────────────────

bool BusEndpoint::handleMessage(int peer_fd, const uint8_t *data, size_t len) {
    // Verify FlatBuffers envelope
    auto env = GetEnvelope(data);
    if (!env || env->schema_version() != 1) {
        return false; // Invalid envelope
    }

    dispatchMessage(env, data);
    return true;
}

void BusEndpoint::dispatchMessage(const Envelope *env, const uint8_t *data) {
    auto payload_type = env->payload_type();

    // Check for Hello (handshake)
    if (payload_type == Payload_Hello) {
        auto hello = env->payload_as_Hello();
        if (hello) {
            handleHello(0, hello); // peer_fd=0 means self-handled
            return;
        }
    }

    // Check for Subscribe
    if (payload_type == Payload_Subscribe) {
        auto sub = env->payload_as_Subscribe();
        if (sub) {
            handleSubscribe(0, sub);
            return;
        }
    }

    // Dispatch to registered handler
    auto it = handlers_.find(payload_type);
    if (it != handlers_.end()) {
        it->second(env, data);
    }
}

void BusEndpoint::handleHello(int /*peer_fd*/, const Hello *hello) { (void)hello; }

void BusEndpoint::handleSubscribe(int /*peer_fd*/, const Subscribe *sub) { (void)sub; }

Result BusEndpoint::sendReliable(int peer_fd, flatbuffers::FlatBufferBuilder &envelope) {
    auto it = peers_.find(peer_fd);
    if (it == peers_.end()) {
        return Result::err(make_error(Error::Code::NotFound, "Peer not found: fd %d", peer_fd));
    }

    PeerConnection &peer = it->second;

    // Serialize the envelope (copy)
    flatbuffers::FlatBufferBuilder copy;
    copy.Reset();
    std::memcpy(copy.GetBufferPointer(), envelope.GetBufferPointer(), envelope.GetSize());

    PendingAck ack;
    ack.builder = std::move(copy);
    ack.first_attempt_ns = monotonic_ns();
    ack.attempt_count = 0;

    peer.reliable_pending.push_back(std::move(ack));

    // Try to send immediately
    flushPending(peer_fd);

    return Result::ok();
}

void BusEndpoint::publishNotify(flatbuffers::FlatBufferBuilder &envelope) {
    for (auto &[fd, peer] : peers_) {
        if (!peer.subscribed)
            continue;

        // Serialize with length prefix
        uint32_t len = static_cast<uint32_t>(envelope.GetSize());
        uint32_t net_len = htonl(len);

        win_send(fd, &net_len, 4);
        win_send(fd, envelope.GetBufferPointer(), len);
    }
}

void BusEndpoint::flushPending(int peer_fd) {
    auto it = peers_.find(peer_fd);
    if (it == peers_.end())
        return;

    PeerConnection &peer = it->second;

    while (!peer.reliable_pending.empty()) {
        auto &pending = peer.reliable_pending.front();

        uint32_t msg_len = static_cast<uint32_t>(pending.builder.GetSize());
        if (msg_len == 0) {
            peer.reliable_pending.pop_front();
            continue;
        }

        uint8_t header[4];
        uint32_t net_len = htonl(msg_len);
        memcpy(header, &net_len, 4);

#ifdef _WIN32
        int written = win_send(peer.fd, header, 4);
        if (written < 0) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
                break;
            peer.reliable_pending.pop_front();
            continue;
        }

        written = win_send(peer.fd, pending.builder.GetBufferPointer(), msg_len);
        if (written < 0) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
                break;
            peer.reliable_pending.pop_front();
            continue;
        }

        if (static_cast<uint32_t>(written) < msg_len) {
            break; // Incomplete send
        }
#else
        ssize_t written = win_send(peer.fd, header, 4);
        if (written < 0 && win_is_wouldblock(errno)) {
            break; // Buffer full, will retry on writable
        }

        written = win_send(peer.fd, pending.builder.GetBufferPointer(), msg_len);
        if (written < 0 && win_is_wouldblock(errno)) {
            break; // Partial write, will retry
        }

        if (written < static_cast<ssize_t>(msg_len)) {
            break; // Incomplete send
        }
#endif

        // Message sent successfully — wait for ack
        peer.reliable_pending.pop_front();
    }
}

void BusEndpoint::onReliableRetryTimer() {
    uint64_t now = monotonic_ns();
    retry_scheduled_ = false;

    for (auto &[fd, peer] : peers_) {
        auto &pending = peer.reliable_pending;

        for (auto it = pending.begin(); it != pending.end();) {
            auto &ack = *it;
            uint64_t age_s = (now - ack.first_attempt_ns) / 1000000000ULL;

            // Check deadline
            if (age_s >= RELIABLE_DEADLINE_S) {
                it = pending.erase(it);
                continue;
            }

            // Exponential backoff: 100ms * 2^attempt, capped at 5s
            uint64_t retry_ms = RELIABLE_RETRY_BASE_MS;
            for (int i = 0; i < ack.attempt_count && retry_ms < RELIABLE_RETRY_MAX_MS; ++i) {
                retry_ms *= 2;
            }

            uint64_t last_attempt_age = (now - ack.first_attempt_ns) / 1000000ULL;
            if (last_attempt_age < retry_ms) {
                ++it;
                continue;
            } // Not yet time

            // Resend
            uint32_t msg_len = static_cast<uint32_t>(ack.builder.GetSize());
            uint32_t net_len = htonl(msg_len);
            int w = win_send(fd, &net_len, 4);
            if (w >= 0) {
                w = win_send(fd, ack.builder.GetBufferPointer(), msg_len);
            }

#ifdef _WIN32
            if (w < static_cast<int>(msg_len)) {
#else
            if (w < static_cast<ssize_t>(msg_len)) {
#endif
                // Will retry on next writable event
                ++it;
                continue;
            }

            ack.attempt_count++;
            ++it;
        }
    }
}

void BusEndpoint::scheduleReliableRetry() {
    if (retry_scheduled_)
        return;

    bool has_pending = false;
    for (auto &[fd, peer] : peers_) {
        if (!peer.reliable_pending.empty()) {
            has_pending = true;
            break;
        }
    }

    if (!has_pending)
        return;

    retry_scheduled_ = true;
    reliable_retry_timer_ = reactor_.addTimer(100, [this]() { onReliableRetryTimer(); });
}

void BusEndpoint::cancelReliableRetry() {
    if (reliable_retry_timer_) {
        reactor_.cancelTimer(reliable_retry_timer_);
        reliable_retry_timer_ = 0;
    }
    retry_scheduled_ = false;
}

void BusEndpoint::onPayload(Payload payload_type, PayloadHandler handler) {
    handlers_[payload_type] = std::move(handler);
}

void BusEndpoint::onConnected(ConnectionCallback cb) {
    connection_cb_ = std::move(cb);
}

int BusEndpoint::connectedPeerFd() const {
    for (const auto &[fd, peer] : peers_) {
        (void)peer;
        if (peer.connected_handled) {
            return fd;
        }
    }
    return -1;
}

} // namespace camera
