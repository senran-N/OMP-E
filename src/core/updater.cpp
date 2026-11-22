#include "core/updater.h"
#include "core/json.h"
#include "core/platform.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#else
#include <cstdlib>
#endif

namespace fs = std::filesystem;

namespace ompe {
namespace {

std::string to_lower_str(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    return out;
}

bool ends_with_ci(std::string_view s, std::string_view suffix) {
    if (s.size() < suffix.size()) return false;
    return to_lower_str(s.substr(s.size() - suffix.size())) == to_lower_str(suffix);
}

int score_platform_installer_asset(std::string_view name) {
    const std::string low = to_lower_str(name);
    if (low.find("sha256") != std::string::npos || low.find(".sig") != std::string::npos) return -1;
#ifdef _WIN32
    const bool is_arm = (low.find("arm64") != std::string::npos);
#if defined(_M_ARM64) || defined(__aarch64__)
    if (!is_arm && (low.find("x64") != std::string::npos || low.find("win64") != std::string::npos)) return -1;
#else
    if (is_arm) return -1;
#endif
    if (ends_with_ci(low, ".exe") && (low.find("setup") != std::string::npos || low.find("installer") != std::string::npos)) return 100;
    if (ends_with_ci(low, ".msi")) return 90;
    if (ends_with_ci(low, ".exe")) return 70;
    return -1;
#elif defined(__APPLE__)
    if (ends_with_ci(low, ".dmg")) return 100;
    if (ends_with_ci(low, ".pkg")) return 90;
    return -1;
#else
    if (ends_with_ci(low, ".appimage")) return 100;
    if (ends_with_ci(low, ".deb")) return 90;
    if (ends_with_ci(low, ".tar.gz")) return 60;
    return -1;
#endif
}

#ifdef _WIN32
std::wstring utf8_to_wide_upd(std::string_view s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

bool winhttp_fetch(const std::string& url,
                   const std::string& user_agent,
                   std::string* out_body,
                   const fs::path* out_file,
                   const std::atomic<bool>& cancel,
                   const std::function<void(uint64_t done, uint64_t total)>& on_progress,
                   std::string& out_err) {
    const std::wstring wurl = utf8_to_wide_upd(url);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
        out_err = "Invalid update URL";
        return false;
    }

    const std::wstring wua = utf8_to_wide_upd(user_agent);
    HINTERNET hSession = WinHttpOpen(wua.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        hSession = WinHttpOpen(wua.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!hSession) {
        out_err = "WinHTTP session failed";
        return false;
    }

    DWORD timeout_ms = 12000;
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    DWORD recv_timeout = out_file ? 60000 : 12000;
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &recv_timeout, sizeof(recv_timeout));

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        out_err = "Could not connect to update server";
        return false;
    }

    const DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, nullptr,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        out_err = "Could not create HTTP request";
        return false;
    }

    DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));

    const wchar_t* headers = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    BOOL ok = WinHttpSendRequest(hRequest, out_file ? WINHTTP_NO_ADDITIONAL_HEADERS : headers,
                                 out_file ? 0 : static_cast<DWORD>(-1L),
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);
    if (!ok) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        out_err = "Network request failed (offline or blocked)";
        return false;
    }

    DWORD status = 0;
    DWORD status_sz = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_sz, WINHTTP_NO_HEADER_INDEX);
    if (status < 200 || status >= 300) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        out_err = (status == 404) ? "No published releases yet" : ("HTTP status " + std::to_string(status));
        return false;
    }

    uint64_t total_bytes = 0;
    wchar_t cl_buf[64] = {};
    DWORD cl_sz = sizeof(cl_buf);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH,
                            WINHTTP_HEADER_NAME_BY_INDEX, cl_buf, &cl_sz, WINHTTP_NO_HEADER_INDEX)) {
        total_bytes = _wcstoui64(cl_buf, nullptr, 10);
    }

    std::ofstream ofs;
    if (out_file) {
        std::error_code ec;
        fs::create_directories(out_file->parent_path(), ec);
        ofs.open(*out_file, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            out_err = "Cannot open temporary file for download";
            return false;
        }
    }

    std::vector<char> buf(65536);
    uint64_t downloaded = 0;
    bool success = true;
    while (!cancel.load()) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
            success = false;
            out_err = "Connection interrupted during download";
            break;
        }
        if (avail == 0) break;
        const DWORD to_read = (avail < buf.size()) ? avail : static_cast<DWORD>(buf.size());
        DWORD got = 0;
        if (!WinHttpReadData(hRequest, buf.data(), to_read, &got)) {
            success = false;
            out_err = "Read error during download";
            break;
        }
        if (got == 0) break;
        if (out_file) {
            ofs.write(buf.data(), static_cast<std::streamsize>(got));
            if (!ofs.good()) {
                success = false;
                out_err = "Disk write failed";
                break;
            }
        } else if (out_body) {
            out_body->append(buf.data(), got);
            if (out_body->size() > 2u * 1024u * 1024u) {
                success = false;
                out_err = "Release metadata exceeded 2 MB limit";
                break;
            }
        }
        downloaded += got;
        if (on_progress) on_progress(downloaded, total_bytes);
    }

    if (cancel.load()) {
        success = false;
        out_err = "Cancelled";
    }
    if (out_file) ofs.close();
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

std::string compute_file_sha256_hex(const fs::path& file_path) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return {};

    DWORD hash_obj_len = 0, cb_data = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hash_obj_len), sizeof(DWORD), &cb_data, 0);
    std::vector<BYTE> hash_obj(hash_obj_len);
    if (BCryptCreateHash(hAlg, &hHash, hash_obj.data(), hash_obj_len, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    std::ifstream f(file_path, std::ios::binary);
    if (!f) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }
    std::vector<char> buf(65536);
    while (f) {
        f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const std::streamsize n = f.gcount();
        if (n > 0) {
            BCryptHashData(hHash, reinterpret_cast<PUCHAR>(buf.data()), static_cast<ULONG>(n), 0);
        }
    }

    BYTE digest[32] = {};
    const bool ok = (BCryptFinishHash(hHash, digest, sizeof(digest), 0) == 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!ok) return {};

    static constexpr char kHex[] = "0123456789abcdef";
    std::string out(64, '0');
    for (int i = 0; i < 32; ++i) {
        out[static_cast<size_t>(i * 2)]     = kHex[(digest[i] >> 4) & 0xF];
        out[static_cast<size_t>(i * 2 + 1)] = kHex[digest[i] & 0xF];
    }
    return out;
}
#endif

} // namespace

SemVer SemVer::parse(std::string_view raw) noexcept {
    SemVer v{};
    while (!raw.empty() && (raw.front() == ' ' || raw.front() == '\t' || raw.front() == 'v' || raw.front() == 'V')) {
        raw.remove_prefix(1);
    }
    if (raw.empty()) return v;

    const size_t dash = raw.find('-');
    if (dash != std::string_view::npos) {
        v.prerelease = true;
        raw = raw.substr(0, dash);
    }
    const size_t plus = raw.find('+');
    if (plus != std::string_view::npos) {
        raw = raw.substr(0, plus);
    }

    auto parse_part = [](std::string_view part, int& out_val) noexcept -> bool {
        if (part.empty() || part.size() > 9) return false;
        int acc = 0;
        for (char c : part) {
            if (c < '0' || c > '9') return false;
            acc = acc * 10 + (c - '0');
        }
        out_val = acc;
        return true;
    };

    const size_t d1 = raw.find('.');
    if (d1 == std::string_view::npos) return v;
    const size_t d2 = raw.find('.', d1 + 1);
    if (!parse_part(raw.substr(0, d1), v.major)) return v;
    if (d2 == std::string_view::npos) {
        if (!parse_part(raw.substr(d1 + 1), v.minor)) return v;
        v.patch = 0;
    } else {
        if (!parse_part(raw.substr(d1 + 1, d2 - d1 - 1), v.minor)) return v;
        if (!parse_part(raw.substr(d2 + 1), v.patch)) return v;
    }
    v.valid = true;
    return v;
}

bool SemVer::is_newer_than(const SemVer& other) const noexcept {
    if (!valid || !other.valid) return false;
    if (major != other.major) return major > other.major;
    if (minor != other.minor) return minor > other.minor;
    if (patch != other.patch) return patch > other.patch;
    // Stable release is newer than same-number prerelease
    return !prerelease && other.prerelease;
}

std::string find_sha256_for_asset(std::string_view sums_text, std::string_view target_filename) {
    if (sums_text.empty() || target_filename.empty()) return {};
    const std::string want = to_lower_str(target_filename);
    size_t pos = 0;
    while (pos < sums_text.size()) {
        const size_t nl = sums_text.find('\n', pos);
        std::string_view line = (nl == std::string_view::npos)
            ? sums_text.substr(pos)
            : sums_text.substr(pos, nl - pos);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.remove_suffix(1);
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.remove_prefix(1);
        if (line.size() >= 66) {
            std::string_view hash_sv = line.substr(0, 64);
            bool hex_ok = true;
            for (char c : hash_sv) {
                if (!std::isxdigit(static_cast<unsigned char>(c))) { hex_ok = false; break; }
            }
            if (hex_ok && (line[64] == ' ' || line[64] == '\t')) {
                std::string_view fname = line.substr(64);
                while (!fname.empty() && (fname.front() == ' ' || fname.front() == '\t' || fname.front() == '*')) fname.remove_prefix(1);
                // Strip leading ./ if present
                if (fname.rfind("./", 0) == 0) fname.remove_prefix(2);
                if (to_lower_str(fname) == want) {
                    return to_lower_str(hash_sv);
                }
            }
        }
        if (nl == std::string_view::npos) break;
        pos = nl + 1;
    }
    return {};
}

bool parse_github_release_json(std::string_view json_text,
                               std::string_view current_version,
                               UpdateSnapshot& out_snap) {
    Json root;
    if (!Json::parse(json_text, root) || !root.is_object()) return false;
    if (root["draft"].as_bool(false) || root["prerelease"].as_bool(false)) {
        out_snap.state = UpdateState::UpToDate;
        out_snap.current_version = std::string(current_version);
        out_snap.latest_version = std::string(current_version);
        return true;
    }

    const std::string tag = root["tag_name"].as_string_copy();
    const SemVer cur_v = SemVer::parse(current_version);
    const SemVer lat_v = SemVer::parse(tag);
    if (!lat_v.valid) return false;

    out_snap.current_version = std::string(current_version);
    out_snap.latest_version = (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) ? tag.substr(1) : tag;
    out_snap.release_title = root["name"].as_string_copy(tag);
    out_snap.release_notes = root["body"].as_string_copy();
    out_snap.release_url = root["html_url"].as_string_copy();
    out_snap.asset_name.clear();
    out_snap.asset_url.clear();
    out_snap.sha256_url.clear();
    out_snap.asset_size_bytes = 0;

    int best_score = -1;
    if (root["assets"].is_array()) {
        for (const auto& a : root["assets"].as_array()) {
            if (!a.is_object()) continue;
            const std::string name = a["name"].as_string_copy();
            const std::string url = a["browser_download_url"].as_string_copy();
            if (name.empty() || url.empty()) continue;
            const std::string low = to_lower_str(name);
            if (low == "sha256sums.txt" || low == "checksums.txt") {
                out_snap.sha256_url = url;
                continue;
            }
            const int sc = score_platform_installer_asset(name);
            if (sc > best_score) {
                best_score = sc;
                out_snap.asset_name = name;
                out_snap.asset_url = url;
                out_snap.asset_size_bytes = static_cast<uint64_t>(a["size"].as_int(0));
            }
        }
    }

    out_snap.state = lat_v.is_newer_than(cur_v) ? UpdateState::Available : UpdateState::UpToDate;
    return true;
}

UpdateManager::~UpdateManager() {
    shutdown();
}

void UpdateManager::set_wake_callback(std::function<void()> cb) {
    std::lock_guard<std::mutex> lk(mu_);
    wake_cb_ = std::move(cb);
}

void UpdateManager::notify_wake() {
    std::function<void()> cb;
    {
        std::lock_guard<std::mutex> lk(mu_);
        cb = wake_cb_;
    }
    if (cb) cb();
}

void UpdateManager::join_worker() {
    cancel_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }
    cancel_.store(false);
}

void UpdateManager::shutdown() {
    join_worker();
}

UpdateSnapshot UpdateManager::snapshot() const {
    std::lock_guard<std::mutex> lk(mu_);
    return snap_;
}

void UpdateManager::check_for_updates(std::string current_version, std::string github_repo) {
    join_worker();
    {
        std::lock_guard<std::mutex> lk(mu_);
        repo_ = std::move(github_repo);
        snap_.state = UpdateState::Checking;
        snap_.current_version = current_version;
        snap_.error_message.clear();
        snap_.download_progress = 0.0f;
    }
    notify_wake();

    const std::string api_url = "https://api.github.com/repos/" + repo_ + "/releases/latest";
    const std::string ua = "OMP-Desktop/" + current_version;

    worker_ = std::thread([this, api_url, ua, current_version]() {
        std::string body;
        std::string err;
        bool ok = false;
#ifdef _WIN32
        ok = winhttp_fetch(api_url, ua, &body, nullptr, cancel_, nullptr, err);
#else
        err = "Direct check uses GitHub Releases page on this platform";
        ok = false;
#endif
        if (cancel_.load()) return;

        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!ok) {
                snap_.state = UpdateState::Error;
                snap_.error_message = err;
            } else {
                UpdateSnapshot parsed = snap_;
                if (parse_github_release_json(body, current_version, parsed)) {
                    snap_ = std::move(parsed);
                } else {
                    snap_.state = UpdateState::Error;
                    snap_.error_message = "Unexpected release metadata format";
                }
            }
        }
        notify_wake();
    });
}

void UpdateManager::download_installer() {
    std::string url, name, sha_url, ver;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (snap_.asset_url.empty() || snap_.asset_name.empty()) {
            if (!snap_.release_url.empty()) open_external_url(snap_.release_url);
            return;
        }
        url = snap_.asset_url;
        name = snap_.asset_name;
        sha_url = snap_.sha256_url;
        ver = snap_.current_version;
    }

    join_worker();
    {
        std::lock_guard<std::mutex> lk(mu_);
        snap_.state = UpdateState::Downloading;
        snap_.download_progress = 0.0f;
        snap_.error_message.clear();
        snap_.sha256_verified = false;
    }
    notify_wake();

    worker_ = std::thread([this, url, name, sha_url, ver]() {
        std::error_code ec;
        const fs::path target_dir = fs::temp_directory_path(ec) / "OMP-Desktop-Updates";
        const fs::path target_file = target_dir / name;
        const std::string ua = "OMP-Desktop/" + ver;
        std::string err;
        bool ok = false;
#ifdef _WIN32
        ok = winhttp_fetch(url, ua, nullptr, &target_file, cancel_,
                           [this](uint64_t done, uint64_t total) {
                               {
                                   std::lock_guard<std::mutex> lk(mu_);
                                   const uint64_t denom = (total > 0) ? total : snap_.asset_size_bytes;
                                   snap_.download_progress = (denom > 0)
                                       ? std::clamp(static_cast<float>(done) / static_cast<float>(denom), 0.0f, 1.0f)
                                       : 0.0f;
                               }
                               notify_wake();
                           }, err);
        bool verified = false;
        if (ok && !sha_url.empty() && !cancel_.load()) {
            std::string sums_body;
            std::string sha_err;
            if (winhttp_fetch(sha_url, ua, &sums_body, nullptr, cancel_, nullptr, sha_err)) {
                const std::string expected = find_sha256_for_asset(sums_body, name);
                if (!expected.empty()) {
                    const std::string actual = compute_file_sha256_hex(target_file);
                    if (actual.empty() || actual != expected) {
                        ok = false;
                        err = "Installer SHA-256 checksum mismatch";
                        fs::remove(target_file, ec);
                    } else {
                        verified = true;
                    }
                }
            }
        }
#else
        bool verified = false;
        err = "Please download the installer from the GitHub Releases page";
#endif
        if (cancel_.load()) return;

        {
            std::lock_guard<std::mutex> lk(mu_);
            if (ok) {
                snap_.state = UpdateState::ReadyToInstall;
                snap_.downloaded_path = target_file.string();
                snap_.download_progress = 1.0f;
                snap_.sha256_verified = verified;
            } else {
                snap_.state = UpdateState::Error;
                snap_.error_message = err;
            }
        }
        notify_wake();
    });
}

bool UpdateManager::launch_installer(std::string* out_err) const {
    std::string path;
    {
        std::lock_guard<std::mutex> lk(mu_);
        path = snap_.downloaded_path;
    }
    std::error_code ec;
    if (path.empty() || !fs::is_regular_file(path, ec)) {
        if (out_err) *out_err = "Downloaded installer file not found";
        return false;
    }
#ifdef _WIN32
    const std::wstring wpath = utf8_to_wide_upd(path);
    const HINSTANCE res = ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(res) <= 32) {
        if (out_err) *out_err = "Failed to launch installer";
        return false;
    }
    return true;
#else
    open_external_url(path);
    return true;
#endif
}

} // namespace ompe
