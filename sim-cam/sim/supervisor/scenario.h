#pragma once
// scenario.h — Parse simulation scenario YAML files

#include "value/result.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace camera {

// ── Device scenario entry ────────────────────────────────────────

struct DeviceScenario {
    std::string device_id;       // hex UUID
    std::string tenant_id;       // hex UUID (optional, defaults to device_id)
    double rate_per_hour = 10.0; // event rate from sim source
    double burst_factor = 1.5;   // burst multiplier
    uint32_t jitter_ms = 500;    // timing jitter
    uint32_t cooldown_s = 30;    // eventd cooldown
    uint16_t sensor_id = 1;      // sensor ID
    std::string listen_path;     // Unix socket path
    bool use_sim_event_source = true; // Use SimEventSource vs sensord process
};

// ── Full scenario ────────────────────────────────────────────────

struct Scenario {
    std::vector<DeviceScenario> devices;
    int max_devices = 10; // maximum concurrent devices
    std::string base_dir; // root directory for device trees
};

// ── YAML parser (minimal, handles our specific format) ───────────

class ScenarioParser {
  public:
    // Parse a scenario YAML file. Returns Error on failure.
    static ResultT<Scenario> parse_file(const std::string &path);

  private:
    // Internal state for recursive descent parsing
    struct ParserState {
        const char *data;
        size_t len;
        size_t pos;
        int line;
    };

    static void skip_whitespace(ParserState &s);
    static std::string read_line(ParserState &s);
    static ResultT<Scenario> parse_devices(ParserState &s, int indent);
    static ResultT<DeviceScenario> parse_device(ParserState &s, int base_indent);
    static std::string trim(const std::string &s);
    static bool starts_with(const std::string &s, const char *prefix);
};

} // namespace camera
