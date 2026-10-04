#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <variant>

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

enum class InitializationError {
    LoggerCreationFailure,
    PatternConfigurationFailure
};

struct ConnectionText {
    std::string_view text;
};
struct ConnectionError {
    int errorCode;
};
struct ConnectionRetry {
    double delaySeconds;
};
struct ConnectionDataPacket {
    std::span<std::byte> payload;
};

struct HardwareFailure {
    std::string_view hardwareId;
};
struct SoftwareTimeout {
    int missingHeartbeats;
};

using FailureContext = std::variant<HardwareFailure, SoftwareTimeout>;

struct ConnectionComplexFailure {
    int criticalLevel;
    FailureContext context;
};

using NetworkState = 
    std::variant<ConnectionText, ConnectionError, ConnectionRetry, ConnectionDataPacket, ConnectionComplexFailure>;

template <size_t N>
struct ValidatedFormatString {
    char value[N]{};

    // The explicit initialization constructor is replaced by an aggregate string literal conversion
    constexpr ValidatedFormatString(const char (&str)[N]) {
        std::copy_n(str, N, value);

        // Perform your exact compile-time format validation checks here
        bool has_placeholder = false;
        for (size_t i = 0; i + 1 < N; ++i) {
            if (value[i] == '{' && value[i + 1] == '}') {
                has_placeholder = true;
                break;
            }
        }
        if (!has_placeholder) {
            throw "Format validation failed: You must include at least one placeholder '{}' token.";
        }
    }
};

template <typename T, typename VariantType>
concept SafeFallbackState = 
    (sizeof(T) <= 24) && 
    std::is_trivially_copyable_v<T> && 
    (std::variant_size_v<VariantType> > 2);

template <typename Derived>
class BaseProcessor {
public:
    void log_visit_start() const {
        auto* derived = static_cast<const Derived*>(this);
        derived->log->trace("Processing states dual dispatch step...");
    }
};

struct StateProcessor : public BaseProcessor<StateProcessor> {
    std::shared_ptr<spdlog::logger> log;

    // Explicit Overload: Data Packet + Text
    void operator()(const ConnectionDataPacket& packet, const ConnectionText& msg) const {
        log_visit_start();
        log->info("Dual Match: Processing data packet alongside text: '{}'", msg.text);

        if (!packet.payload.empty()) {
            packet.payload[0] = std::byte{0xAA};
            log->info("Mutated first byte of payload to 0xAA safely.");
        }
    }

    // Explicit Overload: Complex Failure + Primary Error
    void operator()(const ConnectionComplexFailure& failure, const ConnectionError& primaryErr) const {
        log_visit_start();
        log->error(
            "Dual Match Complex: Code {} paired with critical sub-level {}",
            primaryErr.errorCode,
            failure.criticalLevel
        );

        std::visit([this](const auto& nested_type) {
            using T = std::decay_t<decltype(nested_type)>;
            if constexpr (std::is_same_v<T, HardwareFailure>) {
                log->critical("Nested Unpack: Caught HardwareFailure on component: {}", nested_type.hardwareId);
            } else if constexpr (std::is_same_v<T, SoftwareTimeout>) {
                log->critical("Nested Unpack: Caught SoftwareTimeout. Heartbeats dropped: {}", nested_type.missingHeartbeats);
            }
        }, failure.context);
    }

    // Unconstrained Catch-All Fallback Handler
    template <typename T, typename U>
    void operator()(const T& lhs, const U& rhs) const {
        log_visit_start();

        // Move compile-time concept verification inside the body via if constexpr
        if constexpr (SafeFallbackState<T, NetworkState> && SafeFallbackState<U, NetworkState>) {
            log->warn("Dual Match Fallback: Handled clean unmapped lightweight state combination.");
        } else {
            // Safe execution fallback path for heavy, mismatched, or complex type pairings
            log->warn("Dual Match Matrix Fallback: Handled complex or mixed state permutation safety gate.");
        }
    }
};

inline std::expected<std::shared_ptr<spdlog::logger>, InitializationError> create_raw_logger() noexcept {
    auto console_logger = spdlog::get("network_logger");
    if (console_logger) {
        return console_logger;
    }
    console_logger = spdlog::stdout_color_mt("network_logger");
    if (!console_logger) {
        return std::unexpected(InitializationError::LoggerCreationFailure);
    }
    return console_logger;
}

template <ValidatedFormatString Pattern>
std::expected<std::shared_ptr<spdlog::logger>, InitializationError> configure_logger_attributes(
    std::shared_ptr<spdlog::logger> logger
) noexcept {
    if (!logger) {
        return std::unexpected(InitializationError::PatternConfigurationFailure);
    }
    logger->set_pattern(Pattern.value);
    logger->set_level(spdlog::level::trace);
    return logger;
}

