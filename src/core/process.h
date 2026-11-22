#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ompe {

struct ProcessOptions {
    std::string executable;
    std::vector<std::string> args;
    std::string working_dir;
    std::vector<std::pair<std::string, std::string>> env_overrides;
};

// Cross-platform asynchronous UTF-8 line-oriented subprocess (Windows Job Object / POSIX pgroup).
class Subprocess {
public:
    using LineCallback = std::function<void(std::string line)>;
    using ExitCallback = std::function<void(int exit_code)>;

    Subprocess();
    ~Subprocess();

    Subprocess(const Subprocess&) = delete;
    Subprocess& operator=(const Subprocess&) = delete;

    bool start(const ProcessOptions& opts,
               LineCallback on_stdout_line,
               LineCallback on_stderr_line,
               ExitCallback on_exit,
               std::string* out_error = nullptr);

    bool write_line(const std::string& line);
    void close_stdin();
    void stop(uint32_t graceful_ms = 1200);
    bool is_running() const noexcept;
    uint32_t pid() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Run a short synchronous command and capture stdout (for version checks / `omp session lease`).
bool run_command_capture(const ProcessOptions& opts,
                         std::string& out_stdout,
                         int* out_exit_code = nullptr,
                         uint32_t timeout_ms = 4000);

} // namespace ompe
