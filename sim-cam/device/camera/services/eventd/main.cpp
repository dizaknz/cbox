// eventd — Event policy daemon
// Receives SensorTriggered events, applies cooldown/suppression policy,
// mints event IDs, and produces EventRaised messages.

#include "eventd/eventd.h"
#include "reactor/reactor.h"
#include "value/uuid.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using namespace camera;

static void print_usage(const char *prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --sensor-id N        Sensor ID (default: 1)\n"
              << "  --cooldown S         Cooldown in seconds (default: 30)\n"
              << "  --rate N             Events per hour from sim source (default: 10)\n"
              << "  --device-id HEX      Device UUID as 32-char hex string\n"
              << "  --tenant-id HEX      Tenant UUID as 32-char hex string\n"
              << "  --listen PATH        Unix socket path to listen on (default: /tmp/eventd.sock)\n"
              << "  --help               Show this help\n";
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    EventdConfig config{};
    std::string listen_path = "/tmp/eventd.sock";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--sensor-id" && i + 1 < argc) {
            config.sensor_id = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--cooldown" && i + 1 < argc) {
            config.cooldown_s = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--rate" && i + 1 < argc) {
            config.sim_config.rate_per_hour = std::stod(argv[++i]);
        } else if (arg == "--device-id" && i + 1 < argc) {
            config.device_id_str = argv[++i];
        } else if (arg == "--tenant-id" && i + 1 < argc) {
            config.tenant_id_str = argv[++i];
        } else if (arg == "--listen" && i + 1 < argc) {
            listen_path = argv[++i];
        }
    }

    // Generate device/tenant UUIDs if not provided
    if (config.device_id_str.empty()) {
        auto id = generate_uuid_v7();
        std::ostringstream oss;
        for (int j = 0; j < 8; ++j) {
            oss << std::hex << std::setfill('0') << std::setw(2) << ((id.hi >> (56 - j * 8)) & 0xFF);
        }
        for (int j = 0; j < 8; ++j) {
            oss << std::hex << std::setfill('0') << std::setw(2) << ((id.lo >> (56 - j * 8)) & 0xFF);
        }
        config.device_id_str = oss.str();
    }

    if (config.tenant_id_str.empty()) {
        auto id = generate_uuid_v7();
        std::ostringstream oss;
        for (int j = 0; j < 8; ++j) {
            oss << std::hex << std::setfill('0') << std::setw(2) << ((id.hi >> (56 - j * 8)) & 0xFF);
        }
        for (int j = 0; j < 8; ++j) {
            oss << std::hex << std::setfill('0') << std::setw(2) << ((id.lo >> (56 - j * 8)) & 0xFF);
        }
        config.tenant_id_str = oss.str();
    }

    config.listen_path = listen_path;

    // Create reactor and eventd
    Reactor reactor;
    Eventd eventd(reactor);

    auto result = eventd.init(config);
    if (!result.is_ok()) {
        std::cerr << "Failed to initialize eventd: " << result.error().what() << "\n";
        return 1;
    }

    std::cout << "eventd starting (device=" << config.device_id_str << ", cooldown=" << config.cooldown_s
              << "s, rate=" << config.sim_config.rate_per_hour << "/hr)\n";

    eventd.start();
    eventd.run();

    std::cout << "eventd stopped.\n";
    return 0;
}
