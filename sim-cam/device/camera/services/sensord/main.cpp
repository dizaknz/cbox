// sensord — Simulated sensor daemon entry point
// Generates debounced GPIO-style sensor triggers and publishes them
// to eventd via the bus.

#include "sensord/sensord.h"
#include "reactor/reactor.h"
#include <iostream>
#include <string>

using namespace camera;

static void print_usage(const char *prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --sensor-id N        Sensor ID (default: 1)\n"
              << "  --rate N             Events per hour (default: 10)\n"
              << "  --debounce MS        Debounce interval in ms (default: 500)\n"
              << "  --device-id HEX      Device UUID as 32-char hex string\n"
              << "  --tenant-id HEX      Tenant UUID as 32-char hex string\n"
              << "  --eventd PATH        eventd Unix socket path (default: /tmp/eventd.sock)\n"
              << "  --help               Show this help\n";
}

int main(int argc, char *argv[]) {
    SensordConfig config{};

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--sensor-id" && i + 1 < argc) {
            config.sensor_id = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--rate" && i + 1 < argc) {
            config.event_rate_per_hour = std::stod(argv[++i]);
        } else if (arg == "--debounce" && i + 1 < argc) {
            config.debounce_ms = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--device-id" && i + 1 < argc) {
            config.device_id_str = argv[++i];
        } else if (arg == "--tenant-id" && i + 1 < argc) {
            config.tenant_id_str = argv[++i];
        } else if (arg == "--eventd" && i + 1 < argc) {
            config.eventd_path = argv[++i];
        }
    }

    // Create reactor and sensord
    Reactor reactor;
    Sensord sensord(reactor);

    auto result = sensord.init(config);
    if (!result.is_ok()) {
        std::cerr << "Failed to initialize sensord: " << result.error().what() << "\n";
        return 1;
    }

    std::cout << "sensord starting (sensor_id=" << config.sensor_id
              << ", rate=" << config.event_rate_per_hour << "/hr"
              << ", debounce=" << config.debounce_ms << "ms)\n";

    sensord.start();
    sensord.run();

    std::cout << "sensord stopped.\n";
    return 0;
}
