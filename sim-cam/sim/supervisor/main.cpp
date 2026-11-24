// sim-supervisor — Launches and manages multiple simulated devices from a scenario YAML file.

#include "reactor/reactor.h"
#include "supervisor/device.h"
#include "supervisor/scenario.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static void print_usage(const char *prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --scenario FILE      Scenario YAML file (default: scenario.yml)\n"
              << "  --devices N          Number of devices to launch (0 = all in scenario)\n"
              << "  --base-dir DIR       Base directory for device trees (default: ./sim-out)\n"
              << "  --help               Show this help\n";
}

int main(int argc, char *argv[]) {
    std::string scenario_path = "scenario.yml";
    int num_devices = 0; // 0 = launch all
    std::string base_dir = "./sim-out";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--scenario" && i + 1 < argc) {
            scenario_path = argv[++i];
        } else if (arg == "--devices" && i + 1 < argc) {
            num_devices = std::stoi(argv[++i]);
        } else if (arg == "--base-dir" && i + 1 < argc) {
            base_dir = argv[++i];
        }
    }

    // Parse scenario file
    auto scenario_result = camera::ScenarioParser::parse_file(scenario_path);
    if (!scenario_result.ok()) {
        std::cerr << "Failed to parse scenario: " << scenario_result.error().what() << "\n";
        return 1;
    }

    camera::Scenario scenario = std::move(scenario_result.value());

    // Determine number of devices to launch
    int count = (num_devices > 0) ? num_devices : static_cast<int>(scenario.devices.size());
    if (count == 0) {
        std::cerr << "No devices specified in scenario or via --devices.\n";
        return 1;
    }

    std::cout << "=== Sim Camera Supervisor ===\n";
    std::cout << "Scenario: " << scenario_path << "\n";
    std::cout << "Base dir: " << base_dir << "\n";
    std::cout << "Devices:  " << count << "\n\n";

    // Create device instances and launch them
    std::vector<std::unique_ptr<camera::DeviceInstance>> devices;
    int launched = 0;

    for (int i = 0; i < count && i < static_cast<int>(scenario.devices.size()); ++i) {
        auto &dev_scenario = scenario.devices[i];

        // Create device directory tree
        std::string dev_dir = base_dir + "/device-" + std::to_string(i);
        auto device = std::make_unique<camera::DeviceInstance>(dev_scenario, dev_dir);

        auto tree_result = device->create_directory_tree();
        if (!tree_result.ok()) {
            std::cerr << "Failed to create directory tree for device " << i << ": " << tree_result.error().what()
                      << "\n";
            continue;
        }

        // Start eventd
        auto start_result = device->start_eventd();
        if (!start_result.ok()) {
            std::cerr << "Failed to start eventd for device " << i << ": " << start_result.error().what() << "\n";
            continue;
        }

        devices.push_back(std::move(device));
        ++launched;
    }

    if (launched == 0) {
        std::cerr << "Failed to launch any devices.\n";
        return 1;
    }

    std::cout << "\n=== " << launched << " device(s) running ===\n";
    std::cout << "Press Ctrl+C to stop all devices.\n\n";

    // Wait for all devices (simple: sleep indefinitely, cleanup on signal)
    try {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (...) {
        // Ignore
    }

    // Cleanup
    std::cout << "\nStopping all devices...\n";
    for (auto &device : devices) {
        device->stop_eventd();
    }

    std::cout << "Supervisor exiting.\n";
    return 0;
}
