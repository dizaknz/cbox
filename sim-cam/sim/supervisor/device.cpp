#include "supervisor/device.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

namespace camera {

DeviceInstance::DeviceInstance(const DeviceScenario &scenario, const std::string &base_dir)
    : scenario_(scenario), base_dir_(base_dir) {
    socket_path_ = base_dir + "/eventd.sock";
}

DeviceInstance::~DeviceInstance() { stop_eventd(); }

ResultT<DeviceInstance *> DeviceInstance::create_directory_tree() {
    // Create directory structure:
    //   outbox/  — durable message queue (for linkd)
    //   spool/   — clip storage
    //   state/   — persistent state
    //   config/  — device configuration
    //   certs/   — TLS certificates

    std::vector<std::string> dirs = {
        base_dir_ + "/outbox", base_dir_ + "/spool", base_dir_ + "/state", base_dir_ + "/config", base_dir_ + "/certs"
    };

    for (const auto &dir : dirs) {
#ifndef _WIN32
        if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
            return ResultT<DeviceInstance *>::err(
                make_error(Error::Code::IoError, "mkdir(%s): %s", dir.c_str(), strerror(errno))
            );
        }
#else
        if (CreateDirectoryA(dir.c_str(), nullptr) == FALSE && GetLastError() != ERROR_ALREADY_EXISTS) {
            return ResultT<DeviceInstance *>::err(
                make_error(Error::Code::IoError, "CreateDirectory(%s) failed", dir.c_str())
            );
        }
#endif
    }

    // Write device config file
    std::string config_path = base_dir_ + "/config/device.yml";
    std::ofstream cfg(config_path);
    if (!cfg.is_open()) {
        return ResultT<DeviceInstance *>::err(
            make_error(Error::Code::IoError, "Cannot write config: %s", config_path.c_str())
        );
    }

    cfg << "device_id: " << scenario_.device_id << "\n";
    cfg << "tenant_id: " << (scenario_.tenant_id.empty() ? scenario_.device_id : scenario_.tenant_id) << "\n";
    cfg << "sensor_id: " << scenario_.sensor_id << "\n";
    cfg << "cooldown_s: " << scenario_.cooldown_s << "\n";
    cfg << "rate_per_hour: " << scenario_.rate_per_hour << "\n";
    cfg << "burst_factor: " << scenario_.burst_factor << "\n";
    cfg << "jitter_ms: " << scenario_.jitter_ms << "\n";
    cfg.close();

    return ResultT<DeviceInstance *>::ok(this);
}

ResultT<DeviceInstance *> DeviceInstance::start_eventd() {
    // Build command line for eventd
    std::string cmd;
#ifdef _WIN32
    cmd = "eventd.exe";
#else
    cmd = "./eventd";
#endif

    cmd += " --sensor-id=" + std::to_string(scenario_.sensor_id);
    cmd += " --cooldown=" + std::to_string(scenario_.cooldown_s);
    cmd += " --rate=" + std::to_string(scenario_.rate_per_hour);
    cmd += " --device-id=" + scenario_.device_id;
    if (!scenario_.tenant_id.empty()) {
        cmd += " --tenant-id=" + scenario_.tenant_id;
    }
    cmd += " --listen=" + socket_path_;

#ifndef _WIN32
    // Fork and exec
    pid_t child_pid = fork();
    if (child_pid < 0) {
        return ResultT<DeviceInstance *>::err(make_error(Error::Code::IoError, "fork() failed: %s", strerror(errno)));
    }

    if (child_pid == 0) {
        // Child process
        // Change to device directory
        chdir(base_dir_.c_str());

        // Redirect stdout/stderr to files
        int out_fd = open((base_dir_ + "/outbox/eventd.log").c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd >= 0) {
            dup2(out_fd, STDOUT_FILENO);
            dup2(out_fd, STDERR_FILENO);
            close(out_fd);
        }

        // Exec eventd
        char *args[] = {const_cast<char *>(cmd.c_str()), nullptr};
        execv(args[0], args);

        // If exec fails
        _exit(1);
    }

    pid_ = static_cast<int>(child_pid);
#else
    // Windows: CreateProcess
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    char cmd_line[MAX_PATH];
    snprintf(cmd_line, sizeof(cmd_line), "%s", cmd.c_str());

    if (!CreateProcessA(nullptr, cmd_line, nullptr, nullptr, FALSE, 0, nullptr, base_dir_.c_str(), &si, &pi)) {
        return ResultT<DeviceInstance *>::err(
            make_error(Error::Code::IoError, "CreateProcess failed: %lu", GetLastError())
        );
    }

    pid_ = static_cast<int>(pi.dwProcessId);
    process_handle_ = pi.hProcess;
    CloseHandle(pi.hThread);
#endif

    std::cout << "  Device " << scenario_.device_id << ": eventd started (pid=" << pid_ << ")\n";
    return ResultT<DeviceInstance *>::ok(this);
}

void DeviceInstance::stop_eventd() {
    if (pid_ == 0)
        return;

#ifndef _WIN32
    kill(pid_, SIGTERM);
    // TODO: waitpid to reap
#else
    if (process_handle_) {
        TerminateProcess(process_handle_, 0);
        CloseHandle(process_handle_);
        process_handle_ = nullptr;
    }
#endif

    pid_ = 0;
}

} // namespace camera
