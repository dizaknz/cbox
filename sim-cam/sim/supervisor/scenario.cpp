#include "supervisor/scenario.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace camera {

ResultT<Scenario> ScenarioParser::parse_file(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return ResultT<Scenario>::err(make_error(Error::Code::IoError, "Cannot open scenario file: %s", path.c_str()));
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    ParserState state{};
    state.data = content.c_str();
    state.len = content.size();
    state.pos = 0;
    state.line = 1;

    return parse_devices(state, 0);
}

void ScenarioParser::skip_whitespace(ParserState &s) {
    while (s.pos < s.len && (s.data[s.pos] == ' ' || s.data[s.pos] == '\t')) {
        ++s.pos;
    }
}

std::string ScenarioParser::read_line(ParserState &s) {
    std::string line;
    while (s.pos < s.len && s.data[s.pos] != '\n') {
        line += s.data[s.pos++];
    }
    if (s.pos < s.len)
        ++s.pos; // skip newline
    ++s.line;
    return line;
}

bool ScenarioParser::starts_with(const std::string &s, const char *prefix) {
    size_t len = strlen(prefix);
    return s.size() >= len && s.compare(0, len, prefix) == 0;
}

std::string ScenarioParser::trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

ResultT<Scenario> ScenarioParser::parse_devices(ParserState &s, int base_indent) {
    Scenario scenario;
    std::string line;

    while (s.pos < s.len) {
        skip_whitespace(s);
        if (s.pos >= s.len || s.data[s.pos] == '#') {
            if (s.pos < s.len && s.data[s.pos] == '#') {
                // Skip comment lines
                read_line(s);
            }
            continue;
        }

        line = read_line(s);
        std::string trimmed = trim(line);

        if (trimmed.empty())
            continue;

        // Calculate indent level
        int indent = 0;
        for (char c : line) {
            if (c == ' ')
                ++indent;
            else
                break;
        }

        if (starts_with(trimmed, "devices:")) {
            // Parse device list
            auto result = parse_devices(s, base_indent + 2);
            scenario.devices = std::move(result.value().devices);
            return ResultT<Scenario>::ok(std::move(scenario));
        } else if (starts_with(trimmed, "max_devices:")) {
            scenario.max_devices = std::stoi(trim(trimmed.substr(12)));
        } else if (starts_with(trimmed, "base_dir:")) {
            scenario.base_dir = trim(trimmed.substr(9));
            // Remove quotes if present
            if (!scenario.base_dir.empty() && (scenario.base_dir.front() == '"' || scenario.base_dir.front() == '\'')) {
                scenario.base_dir = scenario.base_dir.substr(1, scenario.base_dir.size() - 2);
            }
        } else if (trimmed == "-") {
            // Start of a new device entry
            auto dev_result = parse_device(s, base_indent + 2);
            if (!dev_result.ok())
                return ResultT<Scenario>::err(dev_result.error());
            scenario.devices.push_back(std::move(dev_result.value()));
        } else if (trimmed == "..." || trimmed == "---") {
            break; // YAML document separator
        }
    }

    return ResultT<Scenario>::ok(std::move(scenario));
}

ResultT<DeviceScenario> ScenarioParser::parse_device(ParserState &s, int base_indent) {
    DeviceScenario dev;
    std::string line;

    while (s.pos < s.len) {
        skip_whitespace(s);
        if (s.pos >= s.len)
            break;

        // Check for next top-level item (new device or end of list)
        if (s.data[s.pos] == '-' && base_indent > 0) {
            // This is a new device entry — stop parsing this one
            return ResultT<DeviceScenario>::ok(dev);
        }

        line = read_line(s);
        std::string trimmed = trim(line);

        if (trimmed.empty())
            continue;

        // Find the colon separator
        auto colon_pos = trimmed.find(':');
        if (colon_pos == std::string::npos)
            continue;

        std::string key = trim(trimmed.substr(0, colon_pos));
        std::string value = trim(trimmed.substr(colon_pos + 1));

        // Remove quotes from value
        if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
            value = value.substr(1, value.size() - 2);
        }

        if (key == "device_id") {
            dev.device_id = value;
        } else if (key == "tenant_id") {
            dev.tenant_id = value;
        } else if (key == "rate_per_hour") {
            dev.rate_per_hour = std::stod(value);
        } else if (key == "burst_factor") {
            dev.burst_factor = std::stod(value);
        } else if (key == "jitter_ms") {
            dev.jitter_ms = static_cast<uint32_t>(std::stoi(value));
        } else if (key == "cooldown_s") {
            dev.cooldown_s = static_cast<uint32_t>(std::stoi(value));
        } else if (key == "sensor_id") {
            dev.sensor_id = static_cast<uint16_t>(std::stoi(value));
        } else if (key == "listen_path") {
            dev.listen_path = value;
        } else if (key == "use_sim_event_source") {
            dev.use_sim_event_source = (value == "true" || value == "1" || value == "yes");
        }
    }

    return ResultT<DeviceScenario>::ok(dev);
}

} // namespace camera
