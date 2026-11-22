#include "core/process.h"

#include <windows.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace ompe {
namespace {

std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::wstring quote_win_arg(const std::wstring& arg) {
    if (arg.empty()) return L"\"\"";
    const bool needs = arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
    if (!needs) return arg;
    std::wstring out = L"\"";
    size_t backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
            backslashes = 0;
        } else {
            out.append(backslashes, L'\\');
            backslashes = 0;
            out.push_back(ch);
        }
    }
    out.append(backslashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}

std::wstring build_command_line(const ProcessOptions& opts) {
    std::wstring cmd = quote_win_arg(utf8_to_wide(opts.executable));
    for (const auto& a : opts.args) {
        cmd.push_back(L' ');
        cmd += quote_win_arg(utf8_to_wide(a));
    }
    return cmd;
}

std::wstring build_env_block(const std::vector<std::pair<std::string, std::string>>& overrides) {
    if (overrides.empty()) return {};
    std::vector<std::pair<std::wstring, std::wstring>> entries;
    LPWCH raw = GetEnvironmentStringsW();
    if (raw) {
        for (const wchar_t* p = raw; *p != L'\0'; ) {
            const std::wstring item(p);
            p += item.size() + 1;
            const size_t eq = item.find(L'=', 1);
            if (eq != std::wstring::npos) {
                entries.emplace_back(item.substr(0, eq), item.substr(eq + 1));
            }
        }
        FreeEnvironmentStringsW(raw);
    }
    for (const auto& [k, v] : overrides) {
        const std::wstring wk = utf8_to_wide(k);
        const std::wstring wv = utf8_to_wide(v);
        bool replaced = false;
        for (auto& [ek, ev] : entries) {
            if (_wcsicmp(ek.c_str(), wk.c_str()) == 0) {
                ev = wv;
                replaced = true;
                break;
            }
        }
        if (!replaced) entries.emplace_back(wk, wv);
    }
    std::wstring block;
    for (const auto& [ek, ev] : entries) {
        block += ek;
        block.push_back(L'=');
        block += ev;
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

void read_pipe_lines(HANDLE pipe, Subprocess::LineCallback cb) {
    std::string buf;
    buf.reserve(4096);
    char tmp[8192];
    DWORD read_bytes = 0;
    while (ReadFile(pipe, tmp, sizeof(tmp), &read_bytes, nullptr) && read_bytes > 0) {
        buf.append(tmp, read_bytes);
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
    HANDLE process = nullptr;
    HANDLE job = nullptr;
    HANDLE stdin_wr = nullptr;
    HANDLE stdout_rd = nullptr;
    HANDLE stderr_rd = nullptr;
    std::mutex stdin_mu;
    std::thread stdout_thread;
    std::thread stderr_thread;
    std::thread wait_thread;
    std::atomic<bool> running{false};
    std::atomic<uint32_t> pid{0};
    ExitCallback on_exit;

    void close_handles() {
        {
            std::lock_guard<std::mutex> lk(stdin_mu);
            if (stdin_wr) { CloseHandle(stdin_wr); stdin_wr = nullptr; }
        }
        if (stdout_rd) { CloseHandle(stdout_rd); stdout_rd = nullptr; }
        if (stderr_rd) { CloseHandle(stderr_rd); stderr_rd = nullptr; }
        if (process) { CloseHandle(process); process = nullptr; }
        if (job) { CloseHandle(job); job = nullptr; }
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

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdin_rd = nullptr, stdin_wr = nullptr;
    HANDLE stdout_rd = nullptr, stdout_wr = nullptr;
    HANDLE stderr_rd = nullptr, stderr_wr = nullptr;

    if (!CreatePipe(&stdin_rd, &stdin_wr, &sa, 0) ||
        !CreatePipe(&stdout_rd, &stdout_wr, &sa, 65536) ||
        !CreatePipe(&stderr_rd, &stderr_wr, &sa, 16384)) {
        if (out_error) *out_error = "Failed to create subprocess pipes";
        if (stdin_rd) CloseHandle(stdin_rd);
        if (stdin_wr) CloseHandle(stdin_wr);
        if (stdout_rd) CloseHandle(stdout_rd);
        if (stdout_wr) CloseHandle(stdout_wr);
        if (stderr_rd) CloseHandle(stderr_rd);
        if (stderr_wr) CloseHandle(stderr_wr);
        return false;
    }

    SetHandleInformation(stdin_wr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdout_rd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = stdin_rd;
    si.hStdOutput = stdout_wr;
    si.hStdError = stderr_wr;

    PROCESS_INFORMATION pi{};
    std::wstring cmd = build_command_line(opts);
    std::wstring cwd = utf8_to_wide(opts.working_dir);
    std::wstring env = build_env_block(opts.env_overrides);

    DWORD flags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;
    const BOOL ok = CreateProcessW(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        TRUE,
        flags,
        env.empty() ? nullptr : env.data(),
        cwd.empty() ? nullptr : cwd.c_str(),
        &si,
        &pi);

    CloseHandle(stdin_rd);
    CloseHandle(stdout_wr);
    CloseHandle(stderr_wr);

    if (!ok) {
        const DWORD err = GetLastError();
        if (out_error) {
            *out_error = "CreateProcessW failed for '" + opts.executable + "' (error " + std::to_string(err) + ")";
        }
        CloseHandle(stdin_wr);
        CloseHandle(stdout_rd);
        CloseHandle(stderr_rd);
        return false;
    }

    CloseHandle(pi.hThread);

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        AssignProcessToJobObject(job, pi.hProcess);
    }

    impl_->process = pi.hProcess;
    impl_->job = job;
    impl_->stdin_wr = stdin_wr;
    impl_->stdout_rd = stdout_rd;
    impl_->stderr_rd = stderr_rd;
    impl_->pid.store(static_cast<uint32_t>(pi.dwProcessId));
    impl_->running.store(true);
    impl_->on_exit = std::move(on_exit);

    impl_->stdout_thread = std::thread([pipe = stdout_rd, cb = std::move(on_stdout_line)]() mutable {
        read_pipe_lines(pipe, std::move(cb));
    });
    impl_->stderr_thread = std::thread([pipe = stderr_rd, cb = std::move(on_stderr_line)]() mutable {
        read_pipe_lines(pipe, std::move(cb));
    });
    impl_->wait_thread = std::thread([im = impl_.get()]() {
        WaitForSingleObject(im->process, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(im->process, &code);
        im->running.store(false);
        if (im->on_exit) im->on_exit(static_cast<int>(code));
    });

    return true;
}

bool Subprocess::write_line(const std::string& line) {
    std::lock_guard<std::mutex> lk(impl_->stdin_mu);
    if (!impl_->stdin_wr) return false;
    std::string data = line;
    data.push_back('\n');
    size_t written_total = 0;
    while (written_total < data.size()) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>(data.size() - written_total);
        if (!WriteFile(impl_->stdin_wr, data.data() + written_total, chunk, &written, nullptr) || written == 0) {
            return false;
        }
        written_total += written;
    }
    return true;
}

void Subprocess::close_stdin() {
    std::lock_guard<std::mutex> lk(impl_->stdin_mu);
    if (impl_->stdin_wr) {
        CloseHandle(impl_->stdin_wr);
        impl_->stdin_wr = nullptr;
    }
}

void Subprocess::stop(uint32_t graceful_ms) {
    close_stdin();
    if (impl_->process) {
        if (WaitForSingleObject(impl_->process, graceful_ms) == WAIT_TIMEOUT) {
            if (impl_->job) {
                TerminateJobObject(impl_->job, 1);
            } else {
                TerminateProcess(impl_->process, 1);
            }
            WaitForSingleObject(impl_->process, 1000);
        }
    }
    if (impl_->wait_thread.joinable()) impl_->wait_thread.join();
    if (impl_->stdout_thread.joinable()) impl_->stdout_thread.join();
    if (impl_->stderr_thread.joinable()) impl_->stderr_thread.join();
    impl_->close_handles();
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
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 16384)) return false;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError = wr;

    PROCESS_INFORMATION pi{};
    std::wstring cmd = build_command_line(opts);
    std::wstring cwd = utf8_to_wide(opts.working_dir);
    std::wstring env = build_env_block(opts.env_overrides);

    const BOOL ok = CreateProcessW(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
        env.empty() ? nullptr : env.data(),
        cwd.empty() ? nullptr : cwd.c_str(),
        &si,
        &pi);
    CloseHandle(wr);

    if (!ok) {
        CloseHandle(rd);
        return false;
    }
    CloseHandle(pi.hThread);

    std::thread reader([rd, &out_stdout]() {
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n > 0) {
            out_stdout.append(buf, n);
        }
    });

    const DWORD wait_res = WaitForSingleObject(pi.hProcess, timeout_ms);
    if (wait_res == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 500);
    }
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    if (reader.joinable()) reader.join();
    CloseHandle(rd);
    if (out_exit_code) *out_exit_code = static_cast<int>(code);
    return wait_res != WAIT_TIMEOUT;
}

} // namespace ompe
