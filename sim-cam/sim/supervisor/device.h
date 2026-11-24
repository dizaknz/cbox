#pragma once
// device.h — Device instance management for the simulation supervisor

#include "supervisor/scenario.h"
#include "value/result.h"
#include <memory>
#include <string>
#include <vector>

namespace camera {

// ── Device instance ──────────────────────────────────────────────

class DeviceInstance {
  public:
    DeviceInstance(const DeviceScenario &scenario, const std::string &base_dir);
    ~DeviceInstance();

    // Create the device's directory tree (outbox, spool, state, config, sockets)
    ResultT<DeviceInstance *> create_directory_tree();

    // Start the eventd process for this device
    ResultT<DeviceInstance *> start_eventd();

    // Stop the eventd process
    void stop_eventd();

    // Get the device ID
    const std::string &device_id() const { return scenario_.device_id; }

    // Get the process ID (0 if not running)
    int pid() const { return pid_; }

  private:
    DeviceScenario scenario_;
    std::string base_dir_;
    std::string socket_path_;
    int pid_ = 0;

    // Platform-specific process management
#ifdef _WIN32
    void *process_handle_ = nullptr;
#else
    int pipe_fd_[2] = {-1, -1}; // stdout/stderr pipe
#endif
};

} // namespace camera
