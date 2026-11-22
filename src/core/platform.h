#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ompe {

struct PlatformInfo {
    std::string os_name;
    std::string arch;
    std::string home_dir;
    std::string omp_root_dir;    // ~/.omp (or PI_CODING_AGENT_DIR parent)
    std::string omp_agent_dir;   // ~/.omp/agent
    std::string desktop_dir;     // ~/.omp/desktop
};

PlatformInfo get_platform_info();
std::string find_omp_executable(const std::string& configured_path = "");
std::string query_omp_version(const std::string& omp_exe);
bool open_external_url(std::string_view url);
bool reveal_in_file_manager(std::string_view path);
std::string current_working_dir();
int64_t unix_time_ms();
std::string format_relative_time(int64_t epoch_ms);
std::string format_compact_tokens(uint64_t tokens);
std::string format_usd(double usd);

} // namespace ompe
