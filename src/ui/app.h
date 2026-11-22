#pragma once

#include "core/updater.h"
#include "omp/client.h"
#include "omp/config_store.h"
#include "theme/icons.h"
#include "theme/motion.h"
#include "theme/palette.h"
#include "theme/particles.h"
#include "theme/typography.h"

#include <etch/etch.h>
#include <inlay/inlay.h>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ompe {

enum class InspectorTab : uint8_t {
    Changes = 0,
    Agents  = 1,
    Tree    = 2,
    Btw     = 3,
};

// Priority-ordered Settings Architecture:
//   CORE:   General -> Providers -> Models -> Appearance
//   SYSTEM: Safety  -> MCP & Skills -> Engine & Logs
enum class SettingsTab : uint8_t {
    General    = 0,
    Providers  = 1,
    Models     = 2,
    Appearance = 3,
    Safety     = 4,
    Ecosystem  = 5,
    Engine     = 6,
};

struct CachedInlayDoc {
    inlay::Doc* doc = nullptr;
    uint64_t revision = 0;
    size_t synced_bytes = 0;
};

struct ProviderFormState {
    bool open = false;
    bool editing = false;
    char id[64] = {};
    char base_url[256] = {};
    char api_key[256] = {};
    char models_csv[512] = {};
    int api_index = 0;       // 0=openai-completions, 1=openai-responses, 2=anthropic-messages, 3=google-generative-ai
    int auth_index = 0;      // 0=apiKey, 1=none, 2=oauth
    int discovery_index = 0; // 0=none, 1=openai-models-list, 2=ollama, 3=lm-studio, 4=llama.cpp
    bool reasoning = false;
    std::string error_msg;
};

struct McpFormState {
    bool open = false;
    char name[64] = {};
    char command[256] = {};
    char args[256] = {};
    char url[256] = {};
    int transport_index = 0; // 0=stdio, 1=http, 2=sse
    std::string error_msg;
};

struct AskDraftState {
    std::string request_id;
    std::vector<std::vector<int>> selected_indices;
    std::vector<std::array<char, 256>> custom_inputs;
    char single_input[512] = {};
    int select_index = 0;
};

struct App {
    etch::Context* ctx = nullptr;
    SDL_Window* window = nullptr;
    bool running = true;
    bool headless_mode = false;
    bool showcase_mode = false;

    ConfigStore config;
    I18nCatalog i18n;
    OmpClient client;
    UpdateManager updater;
    FontSlots fonts{};
    AppIcons icons{};
    ParticleSystem particles;

    etch::ShaderSurfaceId ambient_shader = 0;
    float elapsed_s = 0.0f;
    float last_dt = 1.0f / 60.0f;
    etch::Vec2 viewport{1420.0f, 880.0f};
    std::string omp_version_label;
    std::string gpu_driver_label;

    // Layout & panel states (Workbench closed by default; resizable split when open)
    bool sidebar_open = true;
    bool inspector_open = false;
    InspectorTab inspector_tab = InspectorTab::Changes;
    float workbench_width_px = 412.0f;
    std::string focused_workbench_tool_id;
    std::string focused_workbench_diagram;

    bool settings_open = false;
    SettingsTab settings_tab = SettingsTab::General;
    bool about_open = false;
    bool cmd_palette_open = false;
    bool model_popover_open = false;
    bool thinking_popover_open = false;
    bool workspace_popover_open = false;
    bool renaming_session = false;
    bool all_workspaces_filter = false;

    // Stateful smooth disclosure sets (drives ETCH_COLLAPSE + spring_angle chevrons)
    std::unordered_set<std::string> expanded_disclosures;
    std::unordered_set<std::string> initialized_disclosures;

    // Composer buffers & state
    char composer_buf[16384] = {};
    std::vector<ImageAttachment> pending_images;
    std::vector<std::string> attached_files;
    std::string last_predicted_draft;
    int64_t last_predict_sent_ms = 0;
    bool stick_chat_bottom = true;
    uint32_t last_msg_count = 0;

    // Search & secondary input buffers
    char sidebar_search[128] = {};
    char cmd_search[128] = {};
    int cmd_selected_idx = 0;
    char model_search[128] = {};
    char rename_buf[128] = {};
    char btw_input[1024] = {};
    char subagent_steer_buf[512] = {};
    std::string selected_subagent_id;
    std::string selected_tree_node_id;
    bool tree_turns_only = true;
    bool tree_reset_view_req = false;

    enum class PendingWindowAction : uint8_t {
        None = 0,
        Minimize,
        ToggleMaximize,
        ToggleFullscreen,
    };
    PendingWindowAction pending_win_action = PendingWindowAction::None;

    // Settings input buffers
    char omp_path_buf[512] = {};
    char profile_buf[64] = {};
    char role_default_buf[128] = {};
    char role_smol_buf[128] = {};
    char role_slow_buf[128] = {};
    char role_plan_buf[128] = {};
    ProviderFormState provider_form;
    McpFormState mcp_form;
    std::string manage_accounts_provider;
    char oauth_search[64] = {};
    bool oauth_show_all = false;

    // Extension UI / OAuth interactive state
    AskDraftState ask_draft;
    bool oauth_banner_open = false;
    ExtensionUiRequest oauth_banner_req;

    // Cached InlayDoc instances for zero-reparse incremental Markdown streaming
    std::unordered_map<std::string, CachedInlayDoc> doc_cache;

    // Showcase/offline session override
    std::vector<SessionListItem> showcase_sessions;

    // Live typography selection (reloaded safely outside frame)
    bool pending_font_reload = false;
    std::vector<FontChoice> ui_font_choices;
    std::vector<FontChoice> mono_font_choices;
    char custom_ui_font_buf[96] = {};

    // Copy feedback timer
    std::string copied_key;
    float copied_timer_s = 0.0f;

    // Lifecycle & helpers
    bool init(etch::Context* context, SDL_Window* win, bool headless, bool showcase);
    void shutdown();
    void apply_theme();
    void reload_fonts();
    void set_ui_font_pref(std::string_view fam);
    void set_mono_font_pref(std::string_view fam);
    void reload_locales();
    void set_language_pref(std::string_view pref);
    void sync_settings_buffers();
    void start_omp_for_workspace(const std::string& workspace, const std::string& resume_path = "");
    void tick(float dt_seconds);
    void build_frame();

    const Palette& palette() const noexcept { return get_palette(config.prefs().palette); }
    TypeScale type() const noexcept { return TypeScale::from_body(config.prefs().font_size_px); }
    const char* t(std::string_view key, const char* fallback_en) const noexcept { return i18n.get(key, fallback_en); }
    etch::Str ts(std::string_view key, const char* fallback_en) const noexcept { return etch::str_cstr(i18n.get(key, fallback_en)); }
    std::string tf(std::string_view key, const char* fallback_en,
                   std::initializer_list<std::pair<std::string_view, std::string_view>> args) const {
        return i18n.format(key, fallback_en, args);
    }
    bool is_cjk_locale() const noexcept { return i18n.is_cjk_spacing(); }
    inlay::Doc* get_or_sync_doc(const std::string& key, const std::string& utf8, uint64_t revision);
    void clear_doc_cache();

    bool is_disclosure_open(const std::string& key, bool default_open);
    void toggle_disclosure(const std::string& key, bool default_open);
    void open_workbench_tool(const std::string& tool_call_id);
    void open_workbench_diagram(const std::string& mermaid_src);

    void trigger_send(bool force_steer = false);
    void set_thinking_effort(const std::string& level, bool show_toast = false, etch::Vec2 spark_pos = {});
    void cycle_thinking_effort();
    void handle_global_shortcuts();
    void handle_file_dialogs_and_drops();
    void copy_with_feedback(const std::string& key, const std::string& text, etch::Vec2 spark_pos);
};

// Modular UI builders
void build_titlebar(App& app);
void build_sidebar(App& app);
void build_chat_view(App& app);
void build_composer(App& app);
void build_inspector(App& app);
void build_overlays(App& app);
void build_settings_view(App& app);
void build_about_modal(App& app);

// Shared tiny UI helpers
inline uint32_t utf8_to_utf16_offset(std::string_view s, size_t byte_pos) noexcept {
    size_t i = 0;
    uint32_t u16 = 0;
    const size_t lim = (byte_pos < s.size()) ? byte_pos : s.size();
    while (i < lim) {
        const uint8_t b = static_cast<uint8_t>(s[i]);
        if (b < 0x80u) { i += 1; u16 += 1; }
        else if ((b & 0xE0u) == 0xC0u) { i += 2; u16 += 1; }
        else if ((b & 0xF0u) == 0xE0u) { i += 3; u16 += 1; }
        else if ((b & 0xF8u) == 0xF0u) { i += 4; u16 += 2; }
        else { i += 1; u16 += 1; }
    }
    return u16;
}

} // namespace ompe
