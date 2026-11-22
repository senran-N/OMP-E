#pragma once

#include "core/i18n.h"
#include "theme/palette.h"
#include "theme/particles.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace ompe {

struct SessionListItem {
    std::string session_id;
    std::string file_path;
    std::string cwd;
    std::string title;
    std::string first_message;
    std::string status; // complete, interrupted, aborted, error, pending, unknown
    int64_t created_ms = 0;
    int64_t modified_ms = 0;
    uint64_t file_size = 0;
    bool pinned = false;
    bool held_externally = false;
};

struct CustomModelSpec {
    std::string id;
    std::string name;
    bool reasoning = false;
    uint64_t context_window = 128000;
    uint64_t max_tokens = 16384;
};

struct CustomProviderConfig {
    std::string id;              // e.g., "deepseek", "openrouter", "local-vllm"
    std::string base_url;        // e.g., "https://api.deepseek.com/v1"
    std::string api_key;         // literal key, env var name, or empty when auth=="none"
    std::string auth = "apiKey"; // "apiKey" | "none" | "oauth"
    std::string api = "openai-completions"; // "openai-completions" | "openai-responses" | "anthropic-messages" | "google-generative-ai"
    std::string discovery_type;  // "", "openai-models-list", "ollama", "lm-studio", "llama.cpp"
    std::vector<CustomModelSpec> models;
};

struct McpServerConfig {
    std::string name;
    std::string transport; // "stdio" | "http" | "sse"
    std::string command;
    std::vector<std::string> args;
    std::string url;
    std::string scope; // "user" | "project"
    bool disabled = false;
};

struct EcosystemItem {
    std::string name;
    std::string kind;   // "extension" | "skill" | "rule"
    std::string scope;  // "user" | "project"
    std::string path;
};

struct OmpGlobalConfig {
    std::string approval_mode = "yolo"; // always-ask, always-ask-cached, write, yolo
    std::string role_default;
    std::string role_smol;
    std::string role_slow;
    std::string role_plan;
    std::string default_thinking = "high";
};

struct DesktopPrefs {
    PaletteId palette = PaletteId::ObsidianBrass;
    std::string language = "auto"; // "auto" or any BCP-47 locale tag (e.g. "en-US", "zh-CN", "ja-JP")
    ParticleDensity particle_density = ParticleDensity::Signature;
    bool reduced_motion = false;
    bool mica_backdrop = false;
    bool check_updates_on_startup = true;
    float font_size_px = 15.0f;
    std::string ui_font;           // "" = bundled Geist + HarmonyOS, "harmonyos", or system font family
    std::string mono_font;         // "" = bundled Geist Mono, or system monospace family
    std::string omp_executable;
    std::string profile;
    std::string active_workspace;
    std::vector<std::string> recent_workspaces;
    bool sidebar_open = true;
    int window_w = 1420;
    int window_h = 880;
};

class ConfigStore {
public:
    ConfigStore();

    void reload_all(const std::string& workspace_cwd);

    // Desktop prefs (~/.omp/desktop/prefs.json)
    DesktopPrefs& prefs() noexcept { return prefs_; }
    const DesktopPrefs& prefs() const noexcept { return prefs_; }
    bool load_prefs();
    bool save_prefs() const;
    void record_workspace(const std::string& dir);

    // Sessions discovery (bounded head+tail scan of ~/.omp/agent/sessions/*/*.jsonl)
    void scan_sessions();
    const std::vector<SessionListItem>& sessions() const noexcept { return sessions_; }
    std::vector<SessionListItem> sessions_for_workspace(const std::string& cwd) const;
    bool toggle_pin_session(const std::string& session_id);
    bool delete_session_file(const std::string& file_path);

    // Custom providers (~/.omp/agent/models.yml)
    void load_custom_providers();
    const std::vector<CustomProviderConfig>& custom_providers() const noexcept { return providers_; }
    bool upsert_custom_provider(const CustomProviderConfig& cfg, std::string* out_err = nullptr);
    bool delete_custom_provider(const std::string& provider_id, std::string* out_err = nullptr);
    std::string models_yml_path() const;

    // OMP global config (~/.omp/agent/config.yml + `omp config set`)
    void load_omp_config();
    const OmpGlobalConfig& omp_config() const noexcept { return omp_cfg_; }
    OmpGlobalConfig& omp_config_mut() noexcept { return omp_cfg_; }
    bool set_omp_config_value(const std::string& omp_exe,
                              const std::string& key,
                              const std::string& value,
                              std::string* out_err = nullptr);

    // Ecosystem: MCP servers, extensions, skills, rules
    void scan_ecosystem(const std::string& workspace_cwd);
    const std::vector<McpServerConfig>& mcp_servers() const noexcept { return mcp_servers_; }
    const std::vector<EcosystemItem>& ecosystem_items() const noexcept { return ecosystem_items_; }
    bool upsert_mcp_server(const McpServerConfig& srv, std::string* out_err = nullptr);
    bool delete_mcp_server(const std::string& name, std::string* out_err = nullptr);

    std::string agent_dir() const;
    std::string sessions_dir() const;
    std::string mcp_json_path() const;
    std::string config_yml_path() const;
    std::string desktop_locales_dir() const;

private:
    void load_pins();
    bool save_pins() const;
    bool write_models_yml(std::string* out_err) const;
    bool atomic_write_file(const std::string& path, const std::string& content, std::string* out_err) const;

    DesktopPrefs prefs_;
    OmpGlobalConfig omp_cfg_;
    std::unordered_set<std::string> pinned_ids_;
    std::vector<SessionListItem> sessions_;
    std::vector<CustomProviderConfig> providers_;
    std::vector<McpServerConfig> mcp_servers_;
    std::vector<EcosystemItem> ecosystem_items_;
};

} // namespace ompe
