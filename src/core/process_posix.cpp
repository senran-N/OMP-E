#include "core/process.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

namespace ompe {
namespace {

void read_fd_lines(int fd, Subprocess::LineCallback cb) {
    std::string buf;
    buf.reserve(4096);
    char tmp[8192];
    while (true) {
        const ssize_t n = ::read(fd, tmp, sizeof(tmp));
        if (n <= 0) break;
        buf.append(tmp, static_cast<size_t>(n));
        size_t start = 0;
        while (true) {
            const size_t nl = buf.find('\n', start);
            if (nl == std::string::npos) break;
            size_t end = nl;
            if (end > start && buf[end - 1] == '\r') --end;
            if (cb) cb(buf.substr(start, end - start));
            start = nl + 1;
        }
        if (start > 0) buf.erase(0, start);
    }
    if (!buf.empty() && cb) {
        if (buf.back() == '\r') buf.pop_back();
        cb(std::move(buf));
    }
}

} // namespace

struct Subprocess::Impl {
    pid_t child_pid = -1;
    int stdin_fd = -1;
    int stdout_fd = -1;
    int stderr_fd = -1;
    std::mutex stdin_mu;
    std::thread stdout_thread;
    std::thread stderr_thread;
    std::thread wait_thread;
    std::atomic<bool> running{false};
    std::atomic<uint32_t> pid{0};
    ExitCallback on_exit;

    void close_fds() {
        {
            std::lock_guard<std::mutex> lk(stdin_mu);
            if (stdin_fd >= 0) { ::close(stdin_fd); stdin_fd = -1; }
        }
        if (stdout_fd >= 0) { ::close(stdout_fd); stdout_fd = -1; }
        if (stderr_fd >= 0) { ::close(stderr_fd); stderr_fd = -1; }
    }
};

Subprocess::Subprocess() : impl_(std::make_unique<Impl>()) {}
Subprocess::~Subprocess() { stop(500); }

bool Subprocess::start(const ProcessOptions& opts,
                       LineCallback on_stdout_line,
                       LineCallback on_stderr_line,
                       ExitCallback on_exit,
                       std::string* out_error) {
    stop(300);

    int in_p[2] = {-1, -1};
    int out_p[2] = {-1, -1};
    int err_p[2] = {-1, -1};
    if (::pipe(in_p) != 0 || ::pipe(out_p) != 0 || ::pipe(err_p) != 0) {
        if (out_error) *out_error = "pipe() failed";
        return false;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        if (out_error) *out_error = "fork() failed";
        ::close(in_p[0]); ::close(in_p[1]);
        ::close(out_p[0]); ::close(out_p[1]);
        ::close(err_p[0]); ::close(err_p[1]);
        return false;
    }

    if (pid == 0) {
        ::setpgid(0, 0);
        ::dup2(in_p[0], STDIN_FILENO);
        ::dup2(out_p[1], STDOUT_FILENO);
        ::dup2(err_p[1], STDERR_FILENO);
        ::close(in_p[0]); ::close(in_p[1]);
        ::close(out_p[0]); ::close(out_p[1]);
        ::close(err_p[0]); ::close(err_p[1]);

        if (!opts.working_dir.empty()) {
            (void)::chdir(opts.working_dir.c_str());
        }
        for (const auto& [k, v] : opts.env_overrides) {
            ::setenv(k.c_str(), v.c_str(), 1);
        }

        std::vector<char*> argv;
        argv.reserve(opts.args.size() + 2);
        argv.push_back(const_cast<char*>(opts.executable.c_str()));
        for (const auto& a : opts.args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        ::execvp(opts.executable.c_str(), argv.data());
        ::_exit(127);
    }

    ::close(in_p[0]);
    ::close(out_p[1]);
    ::close(err_p[1]);

    impl_->child_pid = pid;
    impl_->stdin_fd = in_p[1];
    impl_->stdout_fd = out_p[0];
    impl_->stderr_fd = err_p[0];
    impl_->pid.store(static_cast<uint32_t>(pid));
    impl_->running.store(true);
    impl_->on_exit = std::move(on_exit);

    impl_->stdout_thread = std::thread([fd = out_p[0], cb = std::move(on_stdout_line)]() mutable {
        read_fd_lines(fd, std::move(cb));
    });
    impl_->stderr_thread = std::thread([fd = err_p[0], cb = std::move(on_stderr_line)]() mutable {
        read_fd_lines(fd, std::move(cb));
    });
    impl_->wait_thread = std::thread([im = impl_.get()]() {
        int status = 0;
        ::waitpid(im->child_pid, &status, 0);
        const int code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        im->running.store(false);
        if (im->on_exit) im->on_exit(code);
    });

    return true;
}

bool Subprocess::write_line(const std::string& line) {
    std::lock_guard<std::mutex> lk(impl_->stdin_mu);
    if (impl_->stdin_fd < 0) return false;
    std::string data = line;
    data.push_back('\n');
    size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = ::write(impl_->stdin_fd, data.data() + sent, data.size() - sent);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

void Subprocess::close_stdin() {
    std::lock_guard<std::mutex> lk(impl_->stdin_mu);
    if (impl_->stdin_fd >= 0) {
        ::close(impl_->stdin_fd);
        impl_->stdin_fd = -1;
    }
}

void Subprocess::stop(uint32_t graceful_ms) {
    close_stdin();
    if (impl_->child_pid > 0 && impl_->running.load()) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(graceful_ms);
        while (impl_->running.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        if (impl_->running.load()) {
            ::kill(-impl_->child_pid, SIGTERM);
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            if (impl_->running.load()) {
                ::kill(-impl_->child_pid, SIGKILL);
            }
        }
    }
    if (impl_->wait_thread.joinable()) impl_->wait_thread.join();
    if (impl_->stdout_thread.joinable()) impl_->stdout_thread.join();
    if (impl_->stderr_thread.joinable()) impl_->stderr_thread.join();
    impl_->close_fds();
    impl_->child_pid = -1;
    impl_->running.store(false);
    impl_->pid.store(0);
}

bool Subprocess::is_running() const noexcept {
    return impl_->running.load();
}

uint32_t Subprocess::pid() const noexcept {
    return impl_->pid.load();
}

bool run_command_capture(const ProcessOptions& opts,
                         std::string& out_stdout,
                         int* out_exit_code,
                         uint32_t timeout_ms) {
    out_stdout.clear();
    int p[2] = {-1, -1};
    if (::pipe(p) != 0) return false;

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(p[0]); ::close(p[1]);
        return false;
    }
    if (pid == 0) {
        ::setpgid(0, 0);
        ::dup2(p[1], STDOUT_FILENO);
        ::dup2(p[1], STDERR_FILENO);
        ::close(p[0]); ::close(p[1]);
        if (!opts.working_dir.empty()) (void)::chdir(opts.working_dir.c_str());
        for (const auto& [k, v] : opts.env_overrides) ::setenv(k.c_str(), v.c_str(), 1);
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(opts.executable.c_str()));
        for (const auto& a : opts.args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        ::execvp(opts.executable.c_str(), argv.data());
        ::_exit(127);
    }
    ::close(p[1]);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    char buf[4096];
    bool timed_out = false;
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            timed_out = true;
            ::kill(-pid, SIGKILL);
            break;
        }
        const int rem_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        struct pollfd pfd{};
        pfd.fd = p[0];
        pfd.events = POLLIN;
        const int r = ::poll(&pfd, 1, rem_ms);
        if (r > 0) {
            const ssize_t n = ::read(p[0], buf, sizeof(buf));
            if (n <= 0) break;
            out_stdout.append(buf, static_cast<size_t>(n));
        } else if (r == 0) {
            timed_out = true;
            ::kill(-pid, SIGKILL);
            break;
        } else {
            break;
        }
    }
    ::close(p[0]);
    int status = 0;
    ::waitpid(pid, &status, 0);
    if (out_exit_code) *out_exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return !timed_out;
}

} // namespace ompe
