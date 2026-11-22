#include "omp/config_store.h"
#include "omp/protocol.h"
#include "core/json.h"
#include "core/platform.h"
#include "core/process.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace ompe {
namespace {

std::string trim_sv(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.remove_suffix(1);
    return std::string(s);
}

std::string unquote_yaml(std::string_view s) {
    std::string t = trim_sv(s);
    if (t.size() >= 2 && ((t.front() == '"' && t.back() == '"') || (t.front() == '\'' && t.back() == '\''))) {
        return t.substr(1, t.size() - 2);
    }
    return t;
}

std::string yaml_quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::string normalize_path_key(const std::string& p) {
    if (p.empty()) return {};
    std::error_code ec;
    fs::path fp = fs::weakly_canonical(fs::path(p), ec);
    std::string s = ec ? p : fp.string();
    std::replace(s.begin(), s.end(), '\\', '/');
    while (s.size() > 1 && s.back() == '/') s.pop_back();
#ifdef _WIN32
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    return s;
}

bool read_file_text(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool parse_session_file_bounded(const fs::path& file_path, SessionListItem& out) {
    std::error_code ec;
    const uint64_t fsize = fs::file_size(file_path, ec);
    if (ec || fsize == 0) return false;

    const auto ftime = fs::last_write_time(file_path, ec);
    if (!ec) {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        out.modified_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sctp.time_since_epoch()).count();
    }
    out.file_path = file_path.string();
    out.file_size = fsize;
    out.status = "unknown";

    // Extract fallback session ID from filename: <timestamp>_<sessionId>.jsonl
    const std::string stem = file_path.stem().string();
    const size_t us = stem.find('_');
    if (us != std::string::npos && us + 1 < stem.size()) {
        out.session_id = stem.substr(us + 1);
    } else {
        out.session_id = stem;
    }

    std::ifstream f(file_path, std::ios::binary);
    if (!f) return false;

    constexpr size_t kHeadBytes = 4096;
    constexpr size_t kTailBytes = 32768;

    std::string head(std::min<size_t>(static_cast<size_t>(fsize), kHeadBytes), '\0');
    f.read(head.data(), static_cast<std::streamsize>(head.size()));
    head.resize(static_cast<size_t>(f.gcount()));

    bool saw_assistant = false;
    auto inspect_head_line = [&](std::string_view line) {
        if (line.empty() || line.front() != '{') return;
        Json j;
        if (!Json::parse(line, j) || !j.is_object()) return;
        const std::string& type = j["type"].as_string();
        if (type == "title") {
            const std::string t = trim_sv(j["title"].as_string());
            if (!t.empty()) out.title = t;
        } else if (type == "session") {
            if (!j["id"].as_string().empty()) out.session_id = j["id"].as_string();
            if (!j["cwd"].as_string().empty()) out.cwd = j["cwd"].as_string();
            if (out.title.empty() && !j["title"].as_string().empty()) out.title = trim_sv(j["title"].as_string());
        } else if (type == "title_change" && !j["title"].as_string().empty()) {
            out.title = trim_sv(j["title"].as_string());
        } else if (type == "message" && j["message"].is_object()) {
            const Json& m = j["message"];
            const std::string& role = m["role"].as_string();
            if (role == "user" && out.first_message.empty()) {
                if (m["content"].is_string()) {
                    out.first_message = trim_sv(strip_system_xml_blocks(m["content"].as_string()));
                } else if (m["content"].is_array()) {
                    for (const auto& blk : m["content"].as_array()) {
                        if (blk["type"].as_string() == "text" && !blk["text"].as_string().empty()) {
                            std::string cleaned = trim_sv(strip_system_xml_blocks(blk["text"].as_string()));
                            if (!cleaned.empty()) {
                                out.first_message = std::move(cleaned);
                                break;
                            }
                        }
                    }
                }
            } else if (role == "assistant") {
                saw_assistant = true;
            }
        }
    };

    size_t pos = 0;
    while (pos < head.size()) {
        const size_t nl = head.find('\n', pos);
        if (nl == std::string::npos) {
            if (fsize <= kHeadBytes) inspect_head_line(std::string_view(head).substr(pos));
            break;
        }
        inspect_head_line(std::string_view(head).substr(pos, nl - pos));
        pos = nl + 1;
    }

    // Tail scan for terminal session status
    std::string tail;
    if (fsize <= kHeadBytes) {
        tail = head;
    } else {
        const uint64_t seek_off = (fsize > kTailBytes) ? (fsize - kTailBytes) : 0;
        f.clear();
        f.seekg(static_cast<std::streamoff>(seek_off), std::ios::beg);
        tail.resize(static_cast<size_t>(fsize - seek_off));
        f.read(tail.data(), static_cast<std::streamsize>(tail.size()));
        tail.resize(static_cast<size_t>(f.gcount()));
    }

    size_t end = tail.size();
    while (end > 0) {
        while (end > 0 && (tail[end - 1] == '\n' || tail[end - 1] == '\r')) --end;
        if (end == 0) break;
        const size_t start = tail.rfind('\n', end - 1);
        const size_t line_start = (start == std::string::npos) ? 0 : (start + 1);
        const std::string_view line = std::string_view(tail).substr(line_start, end - line_start);
        if (!line.empty() && line.front() == '{') {
            Json j;
            if (Json::parse(line, j) && j.is_object()) {
                const std::string& t = j["type"].as_string();
                if (t == "title_change" && out.title.empty() && !j["title"].as_string().empty()) {
                    out.title = trim_sv(j["title"].as_string());
                } else if (t == "message" && j["message"].is_object()) {
                    const Json& m = j["message"];
                    const std::string& role = m["role"].as_string();
                    if (role == "assistant") {
                        saw_assistant = true;
                        const std::string& sr = m["stopReason"].as_string();
                        if (sr == "stop") out.status = "complete";
                        else if (sr == "aborted") out.status = "aborted";
                        else if (sr == "error") out.status = "error";
                        else if (sr == "length" || sr == "toolUse") out.status = "interrupted";
                        else out.status = "complete";
                    } else if (role == "user") {
                        out.status = "pending";
                    } else if (role == "toolResult") {
                        out.status = "interrupted";
                    }
                    break;
                }
            }
        }
        if (start == std::string::npos) break;
        end = start;
    }

    out.first_message = utf8_truncate_ellipsis(out.first_message, 96);
    for (char& c : out.first_message) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    }
    if (out.title.empty()) {
        out.title = !out.first_message.empty() ? out.first_message : ("Session " + out.session_id.substr(0, 8));
    }
    // Filter out empty stub sessions with zero content
    if (!saw_assistant && out.first_message.empty() && out.title.rfind("Session ", 0) == 0) {
        return false;
    }
    return true;
}

} // namespace

ConfigStore::ConfigStore() {
    load_prefs();
}

std::string ConfigStore::agent_dir() const {
    const PlatformInfo info = get_platform_info();
    if (!prefs_.profile.empty() && !info.omp_root_dir.empty()) {
        return (fs::path(info.omp_root_dir) / "profiles" / prefs_.profile / "agent").string();
    }
    return info.omp_agent_dir;
}

std::string ConfigStore::sessions_dir() const {
    return (fs::path(agent_dir()) / "sessions").string();
}

std::string ConfigStore::models_yml_path() const {
    fs::path p = fs::path(agent_dir()) / "models.yml";
    std::error_code ec;
    if (!fs::exists(p, ec)) {
        fs::path alt = fs::path(agent_dir()) / "models.yaml";
        if (fs::exists(alt, ec)) return alt.string();
    }
    return p.string();
}

std::string ConfigStore::config_yml_path() const {
    fs::path p = fs::path(agent_dir()) / "config.yml";
    std::error_code ec;
    if (!fs::exists(p, ec)) {
        fs::path alt = fs::path(agent_dir()) / "config.yaml";
        if (fs::exists(alt, ec)) return alt.string();
    }
    return p.string();
}

std::string ConfigStore::mcp_json_path() const {
    return (fs::path(agent_dir()) / "mcp.json").string();
}

std::string ConfigStore::desktop_locales_dir() const {
    const PlatformInfo info = get_platform_info();
    if (info.desktop_dir.empty()) return {};
    return (fs::path(info.desktop_dir) / "locales").string();
}

bool ConfigStore::atomic_write_file(const std::string& path, const std::string& content, std::string* out_err) const {
    std::error_code ec;
    fs::path target(path);
    if (target.has_parent_path()) {
        fs::create_directories(target.parent_path(), ec);
    }
    fs::path tmp = target;
    tmp += ".ompe.tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            if (out_err) *out_err = "Failed to open temporary file for writing: " + tmp.string();
            return false;
        }
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
        f.flush();
        if (!f.good()) {
            if (out_err) *out_err = "Failed while writing: " + tmp.string();
            return false;
        }
    }
    fs::rename(tmp, target, ec);
    if (ec) {
        // On Windows rename fails if destination exists on older filesystems; remove and retry cleanly
        fs::remove(target, ec);
        ec.clear();
        fs::rename(tmp, target, ec);
        if (ec) {
            if (out_err) *out_err = "Atomic rename failed: " + ec.message();
            fs::remove(tmp, ec);
            return false;
        }
    }
    return true;
}

void ConfigStore::reload_all(const std::string& workspace_cwd) {
    load_pins();
    scan_sessions();
    load_custom_providers();
    load_omp_config();
    scan_ecosystem(workspace_cwd);
}

bool ConfigStore::load_prefs() {
    const PlatformInfo info = get_platform_info();
    if (info.desktop_dir.empty()) return false;
    const std::string path = (fs::path(info.desktop_dir) / "prefs.json").string();
    std::string raw;
    if (!read_file_text(path, raw)) return false;
    Json j;
    if (!Json::parse(raw, j) || !j.is_object()) return false;

    prefs_.palette = palette_from_key(j["palette"].as_string_copy("obsidian-brass").c_str());
    prefs_.language = normalize_locale_tag(j["language"].as_string_copy("auto"));
    const int pd = static_cast<int>(j["particleDensity"].as_int(2));
    prefs_.particle_density = static_cast<ParticleDensity>(std::clamp(pd, 0, 2));
    prefs_.reduced_motion = j["reducedMotion"].as_bool(false);
    prefs_.mica_backdrop = j["micaBackdrop"].as_bool(false);
    prefs_.check_updates_on_startup = j["checkUpdatesOnStartup"].as_bool(true);
    prefs_.font_size_px = std::clamp(static_cast<float>(j["fontSizePx"].as_double(15.0)), 13.0f, 20.0f);
    prefs_.ui_font = j["uiFont"].as_string_copy();
    prefs_.mono_font = j["monoFont"].as_string_copy();
    prefs_.omp_executable = j["ompExecutable"].as_string_copy();
    prefs_.profile = j["profile"].as_string_copy();
    prefs_.active_workspace = j["activeWorkspace"].as_string_copy();
    prefs_.sidebar_open = j["sidebarOpen"].as_bool(true);
    prefs_.window_w = std::clamp(static_cast<int>(j["windowW"].as_int(1420)), 900, 3840);
    prefs_.window_h = std::clamp(static_cast<int>(j["windowH"].as_int(880)), 620, 2160);
    prefs_.recent_workspaces.clear();
    for (const auto& w : j["recentWorkspaces"].as_array()) {
        if (w.is_string() && !w.as_string().empty()) prefs_.recent_workspaces.push_back(w.as_string());
    }
    return true;
}

bool ConfigStore::save_prefs() const {
    const PlatformInfo info = get_platform_info();
    if (info.desktop_dir.empty()) return false;
    Json j = Json::object();
    j["palette"] = palette_key(prefs_.palette);
    j["language"] = prefs_.language.empty() ? std::string("auto") : prefs_.language;
    j["particleDensity"] = static_cast<int>(prefs_.particle_density);
    j["reducedMotion"] = prefs_.reduced_motion;
    j["micaBackdrop"] = prefs_.mica_backdrop;
    j["checkUpdatesOnStartup"] = prefs_.check_updates_on_startup;
    j["fontSizePx"] = static_cast<double>(prefs_.font_size_px);
    j["uiFont"] = prefs_.ui_font;
    j["monoFont"] = prefs_.mono_font;
    j["ompExecutable"] = prefs_.omp_executable;
    j["profile"] = prefs_.profile;
    j["activeWorkspace"] = prefs_.active_workspace;
    j["sidebarOpen"] = prefs_.sidebar_open;
    j["windowW"] = prefs_.window_w;
    j["windowH"] = prefs_.window_h;
    Json rw = Json::array();
    for (const auto& w : prefs_.recent_workspaces) rw.push_back(w);
    j["recentWorkspaces"] = std::move(rw);
    const std::string path = (fs::path(info.desktop_dir) / "prefs.json").string();
    return atomic_write_file(path, j.dump(2) + "\n", nullptr);
}

void ConfigStore::record_workspace(const std::string& dir) {
    if (dir.empty()) return;
    prefs_.active_workspace = dir;
    const std::string norm = normalize_path_key(dir);
    prefs_.recent_workspaces.erase(
        std::remove_if(prefs_.recent_workspaces.begin(), prefs_.recent_workspaces.end(),
                       [&](const std::string& w) { return normalize_path_key(w) == norm; }),
        prefs_.recent_workspaces.end());
    prefs_.recent_workspaces.insert(prefs_.recent_workspaces.begin(), dir);
    if (prefs_.recent_workspaces.size() > 12) prefs_.recent_workspaces.resize(12);
    save_prefs();
}

void ConfigStore::load_pins() {
    pinned_ids_.clear();
    std::string raw;
    if (!read_file_text((fs::path(agent_dir()) / "session-pins.json").string(), raw)) return;
    Json j;
    if (!Json::parse(raw, j) || !j.is_array()) return;
    for (const auto& v : j.as_array()) {
        if (v.is_string()) pinned_ids_.insert(v.as_string());
    }
}

bool ConfigStore::save_pins() const {
    Json arr = Json::array();
    for (const auto& id : pinned_ids_) arr.push_back(id);
    return atomic_write_file((fs::path(agent_dir()) / "session-pins.json").string(), arr.dump(2) + "\n", nullptr);
}

bool ConfigStore::toggle_pin_session(const std::string& session_id) {
    if (session_id.empty()) return false;
    if (pinned_ids_.count(session_id)) pinned_ids_.erase(session_id);
    else pinned_ids_.insert(session_id);
    for (auto& s : sessions_) {
        if (s.session_id == session_id) s.pinned = pinned_ids_.count(session_id) > 0;
    }
    return save_pins();
}

bool ConfigStore::delete_session_file(const std::string& file_path) {
    std::error_code ec;
    if (file_path.empty() || !fs::exists(file_path, ec)) return false;
    const bool removed = fs::remove(file_path, ec);
    if (removed) {
        // Also remove sibling artifacts directory if present
        fs::path art = fs::path(file_path);
        art.replace_extension("");
        if (fs::is_directory(art, ec)) {
            fs::remove_all(art, ec);
        }
        sessions_.erase(
            std::remove_if(sessions_.begin(), sessions_.end(),
                           [&](const SessionListItem& s) { return s.file_path == file_path; }),
            sessions_.end());
    }
    return removed;
}

void ConfigStore::scan_sessions() {
    sessions_.clear();
    load_pins();

    std::error_code ec;
    const fs::path root(sessions_dir());
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return;

    std::unordered_set<std::string> seen_paths;
    for (const auto& bucket : fs::directory_iterator(root, ec)) {
        if (ec) break;
        if (bucket.is_symlink(ec) || !bucket.is_directory(ec)) continue;
        for (const auto& entry : fs::directory_iterator(bucket.path(), ec)) {
            if (ec) { ec.clear(); continue; }
            if (!entry.is_regular_file(ec)) continue;
            if (entry.path().extension() != ".jsonl") continue;
            const std::string canon = normalize_path_key(entry.path().string());
            if (!seen_paths.insert(canon).second) continue;

            SessionListItem item;
            if (parse_session_file_bounded(entry.path(), item)) {
                item.pinned = pinned_ids_.count(item.session_id) > 0;
                sessions_.push_back(std::move(item));
            }
        }
    }

    std::sort(sessions_.begin(), sessions_.end(), [](const SessionListItem& a, const SessionListItem& b) {
        if (a.pinned != b.pinned) return a.pinned > b.pinned;
        return a.modified_ms > b.modified_ms;
    });
}

std::vector<SessionListItem> ConfigStore::sessions_for_workspace(const std::string& cwd) const {
    if (cwd.empty()) return sessions_;
    const std::string target = normalize_path_key(cwd);
    std::vector<SessionListItem> out;
    for (const auto& s : sessions_) {
        if (normalize_path_key(s.cwd) == target) out.push_back(s);
    }
    return out;
}

// ============================================================================
// Custom Providers (`~/.omp/agent/models.yml`)
// ============================================================================

void ConfigStore::load_custom_providers() {
    providers_.clear();
    std::string raw;
    if (!read_file_text(models_yml_path(), raw)) return;

    std::istringstream ss(raw);
    std::string line;
    bool in_providers = false;
    bool in_discovery = false;
    bool in_models = false;
    CustomProviderConfig* cur_prov = nullptr;
    CustomModelSpec* cur_model = nullptr;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string stripped = trim_sv(line);
        if (stripped.empty() || stripped.front() == '#') continue;

        size_t indent = 0;
        while (indent < line.size() && line[indent] == ' ') ++indent;

        if (indent == 0) {
            in_providers = (stripped == "providers:");
            in_discovery = false;
            in_models = false;
            cur_prov = nullptr;
            cur_model = nullptr;
            continue;
        }
        if (!in_providers) continue;

        if (indent == 2 && stripped.back() == ':') {
            CustomProviderConfig cfg;
            cfg.id = unquote_yaml(stripped.substr(0, stripped.size() - 1));
            providers_.push_back(std::move(cfg));
            cur_prov = &providers_.back();
            cur_model = nullptr;
            in_discovery = false;
            in_models = false;
            continue;
        }
        if (!cur_prov) continue;

        if (indent == 4) {
            in_discovery = false;
            in_models = false;
            cur_model = nullptr;
            const size_t colon = stripped.find(':');
            if (colon == std::string::npos) continue;
            const std::string key = trim_sv(stripped.substr(0, colon));
            const std::string val = unquote_yaml(stripped.substr(colon + 1));
            if (key == "baseUrl") cur_prov->base_url = val;
            else if (key == "apiKey") cur_prov->api_key = val;
            else if (key == "auth") cur_prov->auth = val;
            else if (key == "api") cur_prov->api = val;
            else if (key == "discovery") in_discovery = true;
            else if (key == "models") in_models = true;
        } else if (indent >= 6 && in_discovery) {
            const size_t colon = stripped.find(':');
            if (colon != std::string::npos) {
                const std::string key = trim_sv(stripped.substr(0, colon));
                const std::string val = unquote_yaml(stripped.substr(colon + 1));
                if (key == "type") cur_prov->discovery_type = val;
            }
        } else if (indent >= 6 && in_models) {
            std::string item = stripped;
            if (item.rfind("- ", 0) == 0) {
                cur_prov->models.push_back(CustomModelSpec{});
                cur_model = &cur_prov->models.back();
                item = trim_sv(item.substr(2));
            }
            if (!cur_model) continue;
            const size_t colon = item.find(':');
            if (colon != std::string::npos) {
                const std::string key = trim_sv(item.substr(0, colon));
                const std::string val = unquote_yaml(item.substr(colon + 1));
                if (key == "id") cur_model->id = val;
                else if (key == "name") cur_model->name = val;
                else if (key == "reasoning") cur_model->reasoning = (val == "true");
                else if (key == "contextWindow") cur_model->context_window = std::strtoull(val.c_str(), nullptr, 10);
                else if (key == "maxTokens") cur_model->max_tokens = std::strtoull(val.c_str(), nullptr, 10);
            }
        }
    }
}

bool ConfigStore::write_models_yml(std::string* out_err) const {
    std::ostringstream out;
    out << "# OMP Custom Providers (managed cleanly by OMP-E Desktop)\n";
    out << "providers:\n";
    for (const auto& p : providers_) {
        out << "  " << p.id << ":\n";
        if (!p.base_url.empty()) out << "    baseUrl: " << yaml_quote(p.base_url) << "\n";
        if (!p.api.empty()) out << "    api: " << yaml_quote(p.api) << "\n";
        if (!p.auth.empty() && p.auth != "apiKey") {
            out << "    auth: " << yaml_quote(p.auth) << "\n";
        }
        if (!p.api_key.empty()) out << "    apiKey: " << yaml_quote(p.api_key) << "\n";
        if (!p.discovery_type.empty()) {
            out << "    discovery:\n";
            out << "      type: " << yaml_quote(p.discovery_type) << "\n";
        }
        if (!p.models.empty()) {
            out << "    models:\n";
            for (const auto& m : p.models) {
                out << "      - id: " << yaml_quote(m.id) << "\n";
                if (!m.name.empty() && m.name != m.id) out << "        name: " << yaml_quote(m.name) << "\n";
                if (m.reasoning) out << "        reasoning: true\n";
                if (m.context_window > 0) out << "        contextWindow: " << m.context_window << "\n";
                if (m.max_tokens > 0) out << "        maxTokens: " << m.max_tokens << "\n";
            }
        }
    }
    return atomic_write_file(models_yml_path(), out.str(), out_err);
}

bool ConfigStore::upsert_custom_provider(const CustomProviderConfig& cfg, std::string* out_err) {
    if (cfg.id.empty()) {
        if (out_err) *out_err = "Provider ID is required";
        return false;
    }
    if (cfg.base_url.empty()) {
        if (out_err) *out_err = "Base URL is required";
        return false;
    }
    if (cfg.auth == "apiKey" && cfg.api_key.empty() && !cfg.models.empty()) {
        if (out_err) *out_err = "API key (or env var name) is required when auth is apiKey";
        return false;
    }
    auto it = std::find_if(providers_.begin(), providers_.end(),
                           [&](const CustomProviderConfig& p) { return p.id == cfg.id; });
    if (it != providers_.end()) *it = cfg;
    else providers_.push_back(cfg);
    return write_models_yml(out_err);
}

bool ConfigStore::delete_custom_provider(const std::string& provider_id, std::string* out_err) {
    const size_t before = providers_.size();
    providers_.erase(
        std::remove_if(providers_.begin(), providers_.end(),
                       [&](const CustomProviderConfig& p) { return p.id == provider_id; }),
        providers_.end());
    if (providers_.size() == before) return false;
    return write_models_yml(out_err);
}

// ============================================================================
// OMP Global Settings (`~/.omp/agent/config.yml`)
// ============================================================================

void ConfigStore::load_omp_config() {
    omp_cfg_ = OmpGlobalConfig{};
    std::string raw;
    if (!read_file_text(config_yml_path(), raw)) return;

    std::istringstream ss(raw);
    std::string line;
    std::string section;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string stripped = trim_sv(line);
        if (stripped.empty() || stripped.front() == '#') continue;

        size_t indent = 0;
        while (indent < line.size() && line[indent] == ' ') ++indent;
        const size_t colon = stripped.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = trim_sv(stripped.substr(0, colon));
        const std::string val = unquote_yaml(stripped.substr(colon + 1));

        if (indent == 0) {
            if (val.empty()) {
                section = key;
            } else {
                section.clear();
                if (key == "defaultThinkingLevel") omp_cfg_.default_thinking = val;
            }
        } else if (indent >= 2) {
            if (section == "tools" && key == "approvalMode") omp_cfg_.approval_mode = val;
            else if (section == "modelRoles") {
                if (key == "default") omp_cfg_.role_default = val;
                else if (key == "smol") omp_cfg_.role_smol = val;
                else if (key == "slow") omp_cfg_.role_slow = val;
                else if (key == "plan") omp_cfg_.role_plan = val;
            }
        }
    }
}

bool ConfigStore::set_omp_config_value(const std::string& omp_exe,
                                       const std::string& key,
                                       const std::string& value,
                                       std::string* out_err) {
    ProcessOptions po;
    po.executable = omp_exe.empty() ? find_omp_executable(prefs_.omp_executable) : omp_exe;
    po.args = {"config", "set", key, value};
    if (!prefs_.profile.empty()) {
        po.args.push_back("--profile");
        po.args.push_back(prefs_.profile);
    }
    std::string out;
    int code = -1;
    const bool ok = run_command_capture(po, out, &code, 4000) && code == 0;
    if (!ok && out_err) {
        *out_err = out.empty() ? "omp config set failed" : trim_sv(out);
    }
    if (ok) load_omp_config();
    return ok;
}

// ============================================================================
// Ecosystem: MCP Servers, Extensions, Skills, Rules
// ============================================================================

void ConfigStore::scan_ecosystem(const std::string& workspace_cwd) {
    mcp_servers_.clear();
    ecosystem_items_.clear();

    auto load_mcp_file = [&](const std::string& path, const std::string& scope) {
        std::string raw;
        if (!read_file_text(path, raw)) return;
        Json j;
        if (!Json::parse(raw, j) || !j.is_object()) return;
        const Json& servers = j.contains("mcpServers") ? j["mcpServers"] : j;
        if (!servers.is_object()) return;
        for (const auto& [name, cfg] : servers.as_object()) {
            if (!cfg.is_object()) continue;
            McpServerConfig srv;
            srv.name = name;
            srv.scope = scope;
            srv.command = cfg["command"].as_string_copy();
            srv.url = cfg["url"].as_string_copy();
            srv.disabled = cfg["disabled"].as_bool(false);
            srv.transport = !srv.url.empty() ? cfg["type"].as_string_copy("http") : "stdio";
            for (const auto& a : cfg["args"].as_array()) srv.args.push_back(a.as_string_copy());
            mcp_servers_.push_back(std::move(srv));
        }
    };

    load_mcp_file(mcp_json_path(), "user");
    if (!workspace_cwd.empty()) {
        load_mcp_file((fs::path(workspace_cwd) / ".omp" / "mcp.json").string(), "project");
    }

    auto scan_dir = [&](const fs::path& dir, const char* kind, const char* scope) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            const std::string name = entry.path().filename().string();
            if (!name.empty() && name.front() == '.') continue;
            ecosystem_items_.push_back({name, kind, scope, entry.path().string()});
        }
    };

    const fs::path ad(agent_dir());
    scan_dir(ad / "extensions", "extension", "user");
    scan_dir(ad / "skills", "skill", "user");
    scan_dir(ad / "rules", "rule", "user");
    if (!workspace_cwd.empty()) {
        const fs::path pd = fs::path(workspace_cwd) / ".omp";
        scan_dir(pd / "extensions", "extension", "project");
        scan_dir(pd / "skills", "skill", "project");
        scan_dir(pd / "rules", "rule", "project");
    }
}

bool ConfigStore::upsert_mcp_server(const McpServerConfig& srv, std::string* out_err) {
    if (srv.name.empty()) {
        if (out_err) *out_err = "MCP server name is required";
        return false;
    }
    Json root = Json::object();
    std::string raw;
    if (read_file_text(mcp_json_path(), raw)) {
        Json parsed;
        if (Json::parse(raw, parsed) && parsed.is_object()) root = std::move(parsed);
    }
    if (!root.contains("mcpServers") || !root["mcpServers"].is_object()) {
        root["mcpServers"] = Json::object();
    }
    Json entry = Json::object();
    if (!srv.url.empty()) {
        entry["type"] = srv.transport.empty() ? "http" : srv.transport;
        entry["url"] = srv.url;
    } else {
        entry["command"] = srv.command;
        Json args = Json::array();
        for (const auto& a : srv.args) args.push_back(a);
        entry["args"] = std::move(args);
    }
    if (srv.disabled) entry["disabled"] = true;
    root["mcpServers"][srv.name] = std::move(entry);
    const bool ok = atomic_write_file(mcp_json_path(), root.dump(2) + "\n", out_err);
    if (ok) scan_ecosystem(prefs_.active_workspace);
    return ok;
}

bool ConfigStore::delete_mcp_server(const std::string& name, std::string* out_err) {
    std::string raw;
    if (!read_file_text(mcp_json_path(), raw)) return false;
    Json root;
    if (!Json::parse(raw, root) || !root.is_object()) return false;
    if (!root.contains("mcpServers") || !root["mcpServers"].is_object()) return false;
    root["mcpServers"].as_object_mut().erase(name);
    const bool ok = atomic_write_file(mcp_json_path(), root.dump(2) + "\n", out_err);
    if (ok) scan_ecosystem(prefs_.active_workspace);
    return ok;
}

} // namespace ompe
