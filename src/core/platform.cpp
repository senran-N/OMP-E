#include "core/platform.h"
#include "core/process.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#    include <windows.h>
#    include <shellapi.h>
#elif defined(__APPLE__)
#    include <unistd.h>
#else
#    include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace ompe {
namespace {

std::string get_env_str(const char* name) {
#ifdef _WIN32
    const int needed = GetEnvironmentVariableA(name, nullptr, 0);
    if (needed <= 0) return {};
    std::string val(static_cast<size_t>(needed - 1), '\0');
    GetEnvironmentVariableA(name, val.data(), static_cast<DWORD>(needed));
    return val;
#else
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
#endif
}

std::string trim_copy(std::string_view sv) {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n')) {
        sv.remove_suffix(1);
    }
    return std::string(sv);
}

#ifdef _WIN32
std::wstring utf8_to_wide(std::string_view s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}
#endif

} // namespace

PlatformInfo get_platform_info() {
    PlatformInfo info;
#ifdef _WIN32
    info.os_name = "Windows";
#elif defined(__APPLE__)
    info.os_name = "macOS";
#else
    info.os_name = "Linux";
#endif

#if defined(__x86_64__) || defined(_M_X64)
    info.arch = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    info.arch = "arm64";
#else
    info.arch = "unknown";
#endif

#ifdef _WIN32
    info.home_dir = get_env_str("USERPROFILE");
    if (info.home_dir.empty()) {
        info.home_dir = get_env_str("HOMEDRIVE") + get_env_str("HOMEPATH");
    }
#else
    info.home_dir = get_env_str("HOME");
#endif

    const std::string pi_agent_dir = get_env_str("PI_CODING_AGENT_DIR");
    if (!pi_agent_dir.empty()) {
        fs::path agent_p(pi_agent_dir);
        info.omp_agent_dir = agent_p.string();
        info.omp_root_dir = agent_p.parent_path().string();
    } else if (!info.home_dir.empty()) {
        fs::path root = fs::path(info.home_dir) / ".omp";
        info.omp_root_dir = root.string();
        info.omp_agent_dir = (root / "agent").string();
    }
    if (!info.omp_root_dir.empty()) {
        info.desktop_dir = (fs::path(info.omp_root_dir) / "desktop").string();
    }
    return info;
}

std::string find_omp_executable(const std::string& configured_path) {
    std::error_code ec;
    if (!configured_path.empty() && fs::exists(configured_path, ec)) {
        return configured_path;
    }

    const PlatformInfo info = get_platform_info();
#ifdef _WIN32
    const std::string local_app = get_env_str("LOCALAPPDATA");
    const std::vector<fs::path> candidates = {
        fs::path(local_app) / "omp" / "omp.exe",
        fs::path(info.home_dir) / ".bun" / "bin" / "omp.exe",
        fs::path(info.home_dir) / ".omp" / "bin" / "omp.exe",
    };
    for (const auto& c : candidates) {
        if (!c.empty() && fs::exists(c, ec)) return c.string();
    }
    // Search PATH
    const std::string path_env = get_env_str("PATH");
    size_t start = 0;
    while (start <= path_env.size()) {
        const size_t sep = path_env.find(';', start);
        const std::string_view dir = (sep == std::string::npos)
            ? std::string_view(path_env).substr(start)
            : std::string_view(path_env).substr(start, sep - start);
        if (!dir.empty()) {
            const fs::path exe = fs::path(dir) / "omp.exe";
            if (fs::exists(exe, ec)) return exe.string();
            const fs::path cmd = fs::path(dir) / "omp.cmd";
            if (fs::exists(cmd, ec)) return cmd.string();
        }
        if (sep == std::string::npos) break;
        start = sep + 1;
    }
    return "omp.exe";
#else
    const std::vector<fs::path> candidates = {
        fs::path(info.home_dir) / ".local" / "bin" / "omp",
        fs::path(info.home_dir) / ".bun" / "bin" / "omp",
        fs::path("/usr/local/bin/omp"),
        fs::path("/opt/homebrew/bin/omp"),
    };
    for (const auto& c : candidates) {
        if (!c.empty() && fs::exists(c, ec)) return c.string();
    }
    const std::string path_env = get_env_str("PATH");
    size_t start = 0;
    while (start <= path_env.size()) {
        const size_t sep = path_env.find(':', start);
        const std::string_view dir = (sep == std::string::npos)
            ? std::string_view(path_env).substr(start)
            : std::string_view(path_env).substr(start, sep - start);
        if (!dir.empty()) {
            const fs::path exe = fs::path(dir) / "omp";
            if (fs::exists(exe, ec)) return exe.string();
        }
        if (sep == std::string::npos) break;
        start = sep + 1;
    }
    return "omp";
#endif
}

std::string query_omp_version(const std::string& omp_exe) {
    ProcessOptions opts;
    opts.executable = omp_exe;
    opts.args = {"--version"};
    std::string out;
    int code = -1;
    if (!run_command_capture(opts, out, &code, 3000) || code != 0) {
        return {};
    }
    return trim_copy(out);
}

bool open_external_url(std::string_view url) {
    if (url.empty()) return false;
#ifdef _WIN32
    const std::wstring wurl = utf8_to_wide(url);
    const auto r = reinterpret_cast<intptr_t>(
        ShellExecuteW(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return r > 32;
#elif defined(__APPLE__)
    ProcessOptions opts;
    opts.executable = "/usr/bin/open";
    opts.args = {std::string(url)};
    std::string dummy;
    return run_command_capture(opts, dummy, nullptr, 2000);
#else
    ProcessOptions opts;
    opts.executable = "xdg-open";
    opts.args = {std::string(url)};
    std::string dummy;
    return run_command_capture(opts, dummy, nullptr, 2000);
#endif
}

bool reveal_in_file_manager(std::string_view path) {
    if (path.empty()) return false;
#ifdef _WIN32
    const std::wstring wpath = utf8_to_wide(path);
    const std::wstring arg = L"/select,\"" + wpath + L"\"";
    const auto r = reinterpret_cast<intptr_t>(
        ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL));
    return r > 32;
#elif defined(__APPLE__)
    ProcessOptions opts;
    opts.executable = "/usr/bin/open";
    opts.args = {"-R", std::string(path)};
    std::string dummy;
    return run_command_capture(opts, dummy, nullptr, 2000);
#else
    std::error_code ec;
    fs::path p(path);
    if (fs::is_regular_file(p, ec)) p = p.parent_path();
    ProcessOptions opts;
    opts.executable = "xdg-open";
    opts.args = {p.string()};
    std::string dummy;
    return run_command_capture(opts, dummy, nullptr, 2000);
#endif
}

std::string current_working_dir() {
    std::error_code ec;
    auto p = fs::current_path(ec);
    return ec ? std::string() : p.string();
}

int64_t unix_time_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string format_relative_time(int64_t epoch_ms) {
    if (epoch_ms <= 0) return "";
    const int64_t now = unix_time_ms();
    int64_t diff_s = (now - epoch_ms) / 1000;
    if (diff_s < 0) diff_s = 0;
    if (diff_s < 45) return "just now";
    if (diff_s < 3600) return std::to_string(diff_s / 60) + "m ago";
    if (diff_s < 86400) return std::to_string(diff_s / 3600) + "h ago";
    if (diff_s < 86400 * 30) return std::to_string(diff_s / 86400) + "d ago";
    return std::to_string(diff_s / (86400 * 30)) + "mo ago";
}

std::string format_compact_tokens(uint64_t tokens) {
    char buf[32];
    if (tokens >= 1000000ull) {
        std::snprintf(buf, sizeof(buf), "%.2fM", static_cast<double>(tokens) / 1000000.0);
    } else if (tokens >= 1000ull) {
        std::snprintf(buf, sizeof(buf), "%.1fk", static_cast<double>(tokens) / 1000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(tokens));
    }
    return std::string(buf);
}

std::string format_usd(double usd) {
    char buf[32];
    if (usd < 0.001 && usd > 0.0) {
        std::snprintf(buf, sizeof(buf), "$%.4f", usd);
    } else if (usd < 0.1) {
        std::snprintf(buf, sizeof(buf), "$%.3f", usd);
    } else {
        std::snprintf(buf, sizeof(buf), "$%.2f", usd);
    }
    return std::string(buf);
}

} // namespace ompe
