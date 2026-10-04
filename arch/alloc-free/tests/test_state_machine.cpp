#include <catch2/catch_test_macros.hpp>
#include "allocation_free_state_machine.hpp"

TEST_CASE("Monadic Initialization Pipeline", "[init]") {
    auto processor_result = create_raw_logger()
                                .and_then(configure_logger_attributes<"[%T] [TEST] {}">)
                                .and_then([](auto console_logger) -> std::expected<StateProcessor, InitializationError> {
                                    return StateProcessor{ .log = console_logger };
                                });

    REQUIRE(processor_result.has_value() == true);
}

TEST_CASE("Span Mutation via Dual Dispatch Visitor", "[mutation][variant]") {
    auto logger = create_raw_logger().and_then(configure_logger_attributes<"{}">).value();
    StateProcessor processor{ .log = logger };

    std::byte localBuffer[] = { std::byte{0x00}, std::byte{0x11} };

    NetworkState stateA = ConnectionDataPacket{ .payload = localBuffer };
    NetworkState stateB = ConnectionText{ .text = "Verify Handshake" };

    // Execute visitation
    std::visit(processor, stateA, stateB);

    // Verify mutations written down the raw memory span boundary executed successfully
    REQUIRE(localBuffer[0] == std::byte{0xAA});
    REQUIRE(localBuffer[1] == std::byte{0x11});
}

TEST_CASE("Concept Constraints Verification", "[meta]") {
    // Compile-time checks enforcing structural safety metrics directly on your types
    STATIC_REQUIRE(SafeFallbackState<ConnectionRetry, NetworkState>);
    STATIC_REQUIRE(SafeFallbackState<ConnectionText, NetworkState>);
    
    // Complex failure types containing nested variant contexts exceed size barriers
    STATIC_REQUIRE_FALSE(SafeFallbackState<ConnectionComplexFailure, NetworkState>);
}

