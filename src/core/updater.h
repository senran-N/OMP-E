#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace ompe {

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool prerelease = false;
    bool valid = false;

    static SemVer parse(std::string_view raw) noexcept;
    bool is_newer_than(const SemVer& other) const noexcept;
};

enum class UpdateState : uint8_t {
    Idle = 0,
    Checking,
    UpToDate,
    Available,
    Downloading,
    ReadyToInstall,
    Error,
};

struct UpdateSnapshot {
    UpdateState state = UpdateState::Idle;
    std::string current_version;
    std::string latest_version;
    std::string release_title;
    std::string release_notes;
    std::string release_url;
    std::string asset_name;
    std::string asset_url;
    std::string sha256_url;
    std::string downloaded_path;
    uint64_t asset_size_bytes = 0;
    float download_progress = 0.0f; // 0..1
    bool sha256_verified = false;
    std::string error_message;
};

// Parse GitHub /releases/latest JSON payload and pick the matching platform installer asset
bool parse_github_release_json(std::string_view json_text,
                               std::string_view current_version,
                               UpdateSnapshot& out_snap);

// Parse a SHA256SUMS.txt file and locate the expected lowercase hex digest for target_filename
std::string find_sha256_for_asset(std::string_view sums_text, std::string_view target_filename);

class UpdateManager {
public:
    UpdateManager() = default;
    ~UpdateManager();

    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;

    void set_wake_callback(std::function<void()> cb);
    void check_for_updates(std::string current_version, std::string github_repo = "senran-N/OMP-E");
    void download_installer();
    bool launch_installer(std::string* out_err = nullptr) const;
    void shutdown();

    UpdateSnapshot snapshot() const;

private:
    void join_worker();
    void notify_wake();

    mutable std::mutex mu_;
    UpdateSnapshot snap_;
    std::string repo_ = "senran-N/OMP-E";
    std::function<void()> wake_cb_;
    std::atomic<bool> cancel_{false};
    std::thread worker_;
};

} // namespace ompe
