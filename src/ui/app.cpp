#include "ui/app.h"
#include "ui/thinking_ui.h"
#include "core/base64.h"
#include "core/platform.h"
#include "omp/mock_replay.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef OMPE_HAS_AMBIENT_SHADER
#    include "omp_ambient_dxil.h"
#    ifdef OMPE_HAS_AMBIENT_SPV
#        include "omp_ambient_spv.h"
#    endif
#    ifdef OMPE_HAS_AMBIENT_MSL
#        include "omp_ambient_msl.h"
#    endif
#endif

namespace fs = std::filesystem;

namespace ompe {
namespace {

constexpr uint32_t kDialogOpenWorkspace = 101u;
constexpr uint32_t kDialogAttachFile    = 102u;
constexpr uint32_t kDialogPickOmpExe    = 103u;

void copy_to_buf(char* dst, size_t cap, const std::string& src) noexcept {
    if (!dst || cap == 0) return;
    const size_t n = std::min(cap - 1, src.size());
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

bool is_image_ext(std::string ext) {
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".gif";
}

std::string mime_for_ext(std::string ext) {
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".webp") return "image/webp";
    if (ext == ".gif") return "image/gif";
    return "image/png";
}

} // namespace

bool App::init(etch::Context* context, SDL_Window* win, bool headless, bool showcase) {
    ctx = context;
    window = win;
    headless_mode = headless;
    showcase_mode = showcase;

    ui_font_choices = discover_ui_fonts();
    mono_font_choices = discover_mono_fonts();
    fonts = load_app_fonts(ctx, config.prefs().ui_font, config.prefs().mono_font);
    copy_to_buf(custom_ui_font_buf, sizeof(custom_ui_font_buf),
                (config.prefs().ui_font == "harmonyos") ? "" : config.prefs().ui_font);
    icons = register_app_icons(ctx);
    reload_locales();

    sidebar_open = config.prefs().sidebar_open;
    particles.set_density(config.prefs().particle_density);
    apply_theme();

    if (SDL_GPUDevice* dev = etch::gpu_device(ctx)) {
        const char* drv = SDL_GetGPUDeviceDriver(dev);
        gpu_driver_label = drv ? drv : "native";
    } else {
        gpu_driver_label = "deferred";
    }

#ifdef OMPE_HAS_AMBIENT_SHADER
    etch::ShaderSurfaceCreateDesc sd{};
    sd.dxil = {g_omp_ambient_dxil, sizeof(g_omp_ambient_dxil), "ps_main"};
#    ifdef OMPE_HAS_AMBIENT_SPV
    sd.spirv = {g_omp_ambient_spv, sizeof(g_omp_ambient_spv), "ps_main"};
#    endif
#    ifdef OMPE_HAS_AMBIENT_MSL
    sd.msl = {g_omp_ambient_msl, sizeof(g_omp_ambient_msl), "ps_main"};
#    endif
    sd.continuous_time = !headless_mode;
    sd.max_fps = 15.0f;
    ambient_shader = etch::create_shader_surface(ctx, sd);
#endif

    // Configure native frameless chrome across Windows / Linux / macOS
    etch::WindowBackdrop backdrop = etch::WindowBackdrop::Opaque;
#ifdef _WIN32
    if (!headless_mode && config.prefs().mica_backdrop) {
        backdrop = etch::WindowBackdrop::SystemMica;
    }
#endif
    etch::set_window_chrome(etch::WindowChromeDesc{
        .enabled = true,
        .resizable = true,
        .border_thickness = 6.0f,
        .corner_size = 12.0f,
        .macos_traffic_lights = true,
        .traffic_light_inset_px = 76.0f,
        .backdrop = backdrop,
    }, window);

    // Resolve initial workspace
    std::string ws = config.prefs().active_workspace;
    std::error_code ec;
    if (ws.empty() || !fs::is_directory(ws, ec)) {
        ws = current_working_dir();
    }
    config.record_workspace(ws);
    config.reload_all(ws);
    sync_settings_buffers();

    client.set_wake_callback([this]() {
        if (ctx) etch::wake_up(ctx);
    });
    updater.set_wake_callback([this]() {
        if (ctx) etch::wake_up(ctx);
    });

    if (showcase_mode) {
        omp_version_label = "omp v18.22.2 (Showcase)";
        populate_showcase_fixture(client);
        populate_showcase_sessions(showcase_sessions);
    } else {
        const std::string exe = find_omp_executable(config.prefs().omp_executable);
        omp_version_label = query_omp_version(exe);
        if (omp_version_label.empty()) omp_version_label = "omp not found";
        start_omp_for_workspace(ws);
        if (!headless_mode && config.prefs().check_updates_on_startup) {
            updater.check_for_updates(OMPE_VERSION_STRING, OMPE_GITHUB_REPO);
        }
    }
    return true;
}

void App::shutdown() {
    updater.shutdown();
    if (!headless_mode) {
        config.prefs().sidebar_open = sidebar_open;
        config.save_prefs();
    }
    client.disconnect();
    clear_doc_cache();
    if (ambient_shader && ctx) {
        etch::destroy_shader_surface(ctx, ambient_shader);
        ambient_shader = 0;
    }
}

void App::apply_theme() {
    const Palette& p = palette();
    etch::set_theme(p.to_etch_theme(), ctx);
    particles.set_density(config.prefs().reduced_motion ? ParticleDensity::Off : config.prefs().particle_density);
    etch::set_time_scale(ctx, config.prefs().reduced_motion ? 0.0f : 1.0f);
    if (ctx) etch::request_redraw(ctx);
}

void App::reload_fonts() {
    pending_font_reload = false;
    if (!ctx) return;
    const FontSlots prev = fonts;
    fonts = load_app_fonts(ctx, config.prefs().ui_font, config.prefs().mono_font, &prev);
    if (!fonts.ui_pref_matched && !config.prefs().ui_font.empty()) {
        client.push_toast(tf("toast.font_not_found", "System font not found: {name}", {{"name", config.prefs().ui_font}}), "warning", 3000);
    } else if (!fonts.mono_pref_matched && !config.prefs().mono_font.empty()) {
        client.push_toast(tf("toast.font_not_found", "System font not found: {name}", {{"name", config.prefs().mono_font}}), "warning", 3000);
    }
    clear_doc_cache();
    etch::request_redraw(ctx);
}

void App::set_ui_font_pref(std::string_view fam) {
    while (!fam.empty() && (fam.front() == ' ' || fam.front() == '\t')) fam.remove_prefix(1);
    while (!fam.empty() && (fam.back() == ' ' || fam.back() == '\t')) fam.remove_suffix(1);
    config.prefs().ui_font = std::string(fam);
    copy_to_buf(custom_ui_font_buf, sizeof(custom_ui_font_buf),
                (config.prefs().ui_font == "harmonyos") ? "" : config.prefs().ui_font);
    if (!headless_mode) config.save_prefs();
    pending_font_reload = true;
    if (ctx) etch::request_redraw(ctx);
}

void App::set_mono_font_pref(std::string_view fam) {
    while (!fam.empty() && (fam.front() == ' ' || fam.front() == '\t')) fam.remove_prefix(1);
    while (!fam.empty() && (fam.back() == ' ' || fam.back() == '\t')) fam.remove_suffix(1);
    config.prefs().mono_font = std::string(fam);
    if (!headless_mode) config.save_prefs();
    pending_font_reload = true;
    if (ctx) etch::request_redraw(ctx);
}

void App::reload_locales() {
    std::vector<std::string> dirs;
    std::error_code ec;
    auto push_if_dir = [&](const fs::path& p) {
        if (!p.empty() && fs::is_directory(p, ec)) {
            const std::string s = p.string();
            if (std::find(dirs.begin(), dirs.end(), s) == dirs.end()) dirs.push_back(s);
        }
    };
    if (const char* base = SDL_GetBasePath()) {
        push_if_dir(fs::path(base) / "assets" / "locales");
    }
    push_if_dir(fs::path("assets") / "locales");
#ifdef OMPE_SOURCE_DIR
    push_if_dir(fs::path(OMPE_SOURCE_DIR) / "assets" / "locales");
#endif
    const int user_idx = static_cast<int>(dirs.size());
    const std::string user_dir = config.desktop_locales_dir();
    i18n.set_user_locales_dir(user_dir);
    if (!user_dir.empty()) {
        push_if_dir(fs::path(user_dir));
    }
    i18n.load_directories(dirs, user_idx);
    i18n.set_locale(config.prefs().language);
}

void App::set_language_pref(std::string_view pref) {
    config.prefs().language = pref.empty() ? "auto" : normalize_locale_tag(pref);
    i18n.set_locale(config.prefs().language);
    if (!headless_mode) config.save_prefs();
    if (ctx) etch::request_redraw(ctx);
}

void App::sync_settings_buffers() {
    copy_to_buf(omp_path_buf, sizeof(omp_path_buf),
                config.prefs().omp_executable.empty() ? find_omp_executable() : config.prefs().omp_executable);
    copy_to_buf(profile_buf, sizeof(profile_buf), config.prefs().profile);
    copy_to_buf(role_default_buf, sizeof(role_default_buf), config.omp_config().role_default);
    copy_to_buf(role_smol_buf, sizeof(role_smol_buf), config.omp_config().role_smol);
    copy_to_buf(role_slow_buf, sizeof(role_slow_buf), config.omp_config().role_slow);
    copy_to_buf(role_plan_buf, sizeof(role_plan_buf), config.omp_config().role_plan);
}

void App::start_omp_for_workspace(const std::string& workspace, const std::string& resume_path) {
    clear_doc_cache();
    config.record_workspace(workspace);
    config.scan_ecosystem(workspace);

    ClientLaunchConfig cfg;
    cfg.omp_executable = find_omp_executable(config.prefs().omp_executable);
    cfg.working_dir = workspace;
    cfg.profile = config.prefs().profile;
    cfg.resume_session_path = resume_path;
    client.connect(cfg);

    // Probe active terminal leases for visible sessions once connected
    std::vector<std::string> ids;
    for (const auto& s : config.sessions()) {
        ids.push_back(s.session_id);
        if (ids.size() >= 48) break;
    }
    if (!ids.empty()) {
        client.request_session_leases(ids);
    }
}

void App::clear_doc_cache() {
    for (auto& [_, entry] : doc_cache) {
        if (entry.doc) inlay_doc_destroy(entry.doc);
    }
    doc_cache.clear();
    expanded_disclosures.clear();
    initialized_disclosures.clear();
    focused_workbench_tool_id.clear();
    focused_workbench_diagram.clear();
}

bool App::is_disclosure_open(const std::string& key, bool default_open) {
    if (initialized_disclosures.insert(key).second && default_open) {
        expanded_disclosures.insert(key);
    }
    return expanded_disclosures.count(key) > 0;
}

void App::toggle_disclosure(const std::string& key, bool default_open) {
    const bool cur = is_disclosure_open(key, default_open);
    if (cur) expanded_disclosures.erase(key);
    else expanded_disclosures.insert(key);
}

void App::open_workbench_tool(const std::string& tool_call_id) {
    focused_workbench_tool_id = tool_call_id;
    focused_workbench_diagram.clear();
    inspector_tab = InspectorTab::Changes;
    inspector_open = true;
}

void App::open_workbench_diagram(const std::string& mermaid_src) {
    focused_workbench_diagram = mermaid_src;
    focused_workbench_tool_id.clear();
    inspector_tab = InspectorTab::Changes;
    inspector_open = true;
}

inlay::Doc* App::get_or_sync_doc(const std::string& key, const std::string& utf8, uint64_t revision) {
    auto& entry = doc_cache[key];
    if (!entry.doc) {
        inlay::DocConfig cfg = inlay::doc_config_default();
        entry.doc = inlay::create_doc(ctx, &cfg);
        entry.revision = 0;
        entry.synced_bytes = 0;
    }
    if (!entry.doc) return nullptr;

    if (entry.revision != revision || entry.synced_bytes != utf8.size()) {
        // Fast incremental append when streaming grows strictly at the tail!
        if (utf8.size() > entry.synced_bytes && entry.synced_bytes > 0) {
            const EtchStr existing = inlay_doc_source(entry.doc);
            if (existing.length == entry.synced_bytes &&
                std::memcmp(existing.chars, utf8.data(), entry.synced_bytes) == 0) {
                const char* tail = utf8.data() + entry.synced_bytes;
                const uint32_t tail_len = static_cast<uint32_t>(utf8.size() - entry.synced_bytes);
                inlay_doc_append_utf8(entry.doc, EtchStr{tail, tail_len, false});
                entry.synced_bytes = utf8.size();
                entry.revision = revision;
                return entry.doc;
            }
        }
        inlay_doc_set_utf8(entry.doc, EtchStr{utf8.data(), static_cast<uint32_t>(utf8.size()), false});
        entry.synced_bytes = utf8.size();
        entry.revision = revision;
    }
    return entry.doc;
}

void App::copy_with_feedback(const std::string& key, const std::string& text, etch::Vec2 spark_pos) {
    etch::set_clipboard_text(text.c_str());
    copied_key = key;
    copied_timer_s = 1.6f;
    particles.spawn_point_spark(spark_pos, palette().accent, 16);
    client.push_toast(t("toast.copied", "Copied to clipboard"), "success", 2000);
}

void App::set_thinking_effort(const std::string& level, bool show_toast, etch::Vec2 spark_pos) {
    if (level.empty()) return;
    client.state().thinking_level = level;
    if (!showcase_mode) {
        client.set_thinking_level(level);
    }
    const ThinkingLevelMeta meta = thinking_level_meta(level, i18n);
    if (spark_pos.x > 1.0f || spark_pos.y > 1.0f) {
        const etch::Color spark_col = (meta.bars == 0)
            ? palette().text_muted
            : (meta.cool_tint ? palette().secondary : palette().accent);
        particles.spawn_point_spark(spark_pos, spark_col, 14);
    }
    if (show_toast) {
        client.push_toast(tf("toast.thinking_effort", "Thinking effort: {level}", {{"level", meta.title}}), "info", 1800);
    }
}

void App::cycle_thinking_effort() {
    const auto& lvls = client.available_thinking_levels();
    if (lvls.size() <= 1) {
        client.push_toast(t("toast.thinking_unsupported", "Current model does not support thinking effort"), "info", 2200);
        return;
    }
    const std::string& cur = client.state().thinking_level;
    size_t next = 0;
    for (size_t i = 0; i < lvls.size(); ++i) {
        if (lvls[i] == cur) {
            next = (i + 1) % lvls.size();
            break;
        }
    }
    const etch::Rect pill_r = etch::element_bounds(etch::id("cmp_think_pill"));
    const etch::Vec2 spark = (pill_r.w > 4.0f)
        ? etch::Vec2{pill_r.x + pill_r.w * 0.5f, pill_r.y + pill_r.h * 0.5f}
        : etch::Vec2{};
    set_thinking_effort(lvls[next], true, spark);
}

void App::trigger_send(bool force_steer) {
    std::string msg = composer_buf;
    // Trim whitespace
    while (!msg.empty() && (msg.front() == ' ' || msg.front() == '\t' || msg.front() == '\r' || msg.front() == '\n')) msg.erase(msg.begin());
    while (!msg.empty() && (msg.back() == ' ' || msg.back() == '\t' || msg.back() == '\r' || msg.back() == '\n')) msg.pop_back();
    if (msg.empty() && pending_images.empty() && attached_files.empty()) return;

    // Prepend attached file @mentions cleanly
    if (!attached_files.empty()) {
        std::string prefix;
        for (const auto& f : attached_files) {
            prefix += "@" + f + " ";
        }
        msg = prefix + msg;
    }

    // Celebrate submission with directional upward mineral dust burst along composer top edge!
    const etch::Rect cr = etch::element_bounds(etch::id("composer_card"));
    if (cr.w > 10.0f) {
        particles.spawn_send_burst({cr.x + 18.0f, cr.y, cr.w - 36.0f, 4.0f}, palette().accent, palette().secondary);
    }

    if (showcase_mode) {
        // Responsive local echo in offline showcase mode
        ChatMessage u;
        u.id = "demo_u_" + std::to_string(unix_time_ms());
        u.role = "user";
        u.timestamp_ms = unix_time_ms();
        u.blocks.push_back({ContentBlockKind::Text, msg});
        client.messages().push_back(std::move(u));
    } else if (force_steer && client.state().is_streaming) {
        client.send_steer(msg, pending_images);
    } else {
        client.send_prompt(msg, pending_images, client.state().is_streaming ? "followUp" : "");
    }

    composer_buf[0] = '\0';
    pending_images.clear();
    attached_files.clear();
    client.clear_ghost_prediction();
    stick_chat_bottom = true;
}

void App::tick(float dt_seconds) {
    last_dt = dt_seconds;
    elapsed_s += dt_seconds;

    if (copied_timer_s > 0.0f) {
        copied_timer_s = std::max(0.0f, copied_timer_s - dt_seconds);
        if (copied_timer_s <= 0.0f) copied_key.clear();
        etch::request_redraw(ctx);
    }

    if (!showcase_mode) {
        client.poll();
    }
    client.prune_toasts();

    // Restore editor draft if OMP sent `abort_and_restore_queue` or `set_editor_text`
    std::string restored;
    if (client.take_pending_editor_text(restored)) {
        copy_to_buf(composer_buf, sizeof(composer_buf), restored);
        etch::request_focus(etch::id("composer_input"));
    }

    // Trigger subtle completion sparkle when turn settles
    if (client.take_just_settled_flag()) {
        const etch::Rect cr = etch::element_bounds(etch::id("composer_card"));
        if (cr.w > 40.0f) {
            particles.spawn_settle_sparkle({cr.x + 24.0f, cr.y - 10.0f}, cr.w - 48.0f,
                                           palette().accent, palette().secondary);
        }
    }

    if (client.take_session_list_dirty_flag() && !showcase_mode) {
        config.scan_sessions();
    }

    ExtensionUiRequest oauth_req;
    if (client.take_oauth_url_event(oauth_req)) {
        oauth_banner_req = std::move(oauth_req);
        oauth_banner_open = true;
    }

    if (particles.update(dt_seconds)) {
        etch::request_redraw(ctx);
    }

    // Keep live orbital spinners & voice shaders animating ONLY when an animated indicator is actually visible on screen
    const auto upd_state = updater.snapshot().state;
    const bool upd_spinner_visible =
        (upd_state == UpdateState::Checking || upd_state == UpdateState::Downloading) &&
        (about_open || (settings_open && settings_tab == SettingsTab::General));
    bool has_active_spinner = client.state().is_streaming || client.state().is_compacting ||
                              client.status() == ConnectionStatus::Starting ||
                              client.status() == ConnectionStatus::Handshaking ||
                              upd_spinner_visible;
    if (!has_active_spinner) {
        const bool agents_tab_visible = inspector_open && inspector_tab == InspectorTab::Agents;
        for (const auto& s : client.subagents()) {
            if (s.status != "running") continue;
            if (agents_tab_visible) {
                has_active_spinner = true;
                break;
            }
            if (!s.parent_tool_call_id.empty()) {
                for (const auto& m : client.messages()) {
                    for (const auto& b : m.blocks) {
                        if (b.kind == ContentBlockKind::ToolCall && b.tool_call_id == s.parent_tool_call_id) {
                            has_active_spinner = true;
                            break;
                        }
                    }
                    if (has_active_spinner) break;
                }
            }
            if (has_active_spinner) break;
        }
    }
    if (!has_active_spinner) {
        for (const auto& m : client.messages()) {
            for (const auto& b : m.blocks) {
                if ((b.kind == ContentBlockKind::ToolCall || b.kind == ContentBlockKind::Thinking) && !b.complete) {
                    has_active_spinner = true;
                    break;
                }
            }
            if (has_active_spinner) break;
        }
    }
    const bool ambient_visible = !headless_mode && !config.prefs().reduced_motion &&
                                 (client.live_voice_active() || has_active_spinner);
    if (ambient_visible) {
        etch::request_redraw(ctx);
    }
}

void App::handle_global_shortcuts() {
    // Primary+Plus / Primary+Minus / Primary+0: Instant UI & reading font scale zoom
    auto apply_zoom = [&](float next_px) {
        const float clamped = std::clamp(std::round(next_px * 2.0f) * 0.5f, TypeScale::kMinBodyPx, TypeScale::kMaxBodyPx);
        if (std::abs(clamped - config.prefs().font_size_px) > 0.05f) {
            config.prefs().font_size_px = clamped;
            if (!headless_mode) config.save_prefs();
            char px[16];
            std::snprintf(px, sizeof(px), "%.1f", clamped);
            client.push_toast(tf("toast.text_size", "Text size: {px} px", {{"px", px}}), "info", 1600);
            if (ctx) etch::request_redraw(ctx);
        }
    };
    if (etch::consume_shortcut(etch::Mod_Primary, SDLK_EQUALS, true) ||
        etch::consume_shortcut(etch::Mod_Primary, SDLK_KP_PLUS, true)) {
        apply_zoom(config.prefs().font_size_px + 1.0f);
    }
    if (etch::consume_shortcut(etch::Mod_Primary, SDLK_MINUS, true) ||
        etch::consume_shortcut(etch::Mod_Primary, SDLK_KP_MINUS, true)) {
        apply_zoom(config.prefs().font_size_px - 1.0f);
    }
    if (etch::consume_shortcut(etch::Mod_Primary, SDLK_0, true) ||
        etch::consume_shortcut(etch::Mod_Primary, SDLK_KP_0, true)) {
        apply_zoom(TypeScale::kDefaultBodyPx);
    }
    // Primary+Shift+T: Cycle Thinking Effort
    if (etch::consume_shortcut(etch::Mod_Primary | etch::Mod_Shift, SDLK_T, true)) {
        cycle_thinking_effort();
    }
    // Primary+K: Command Palette
    if (etch::consume_shortcut(etch::Mod_Primary, SDLK_K, true)) {
        cmd_palette_open = !cmd_palette_open;
        if (cmd_palette_open) {
            cmd_search[0] = '\0';
            cmd_selected_idx = 0;
            etch::request_focus(etch::id("cmd_palette_input"));
        }
    }
    // Primary+B: Toggle Sidebar
    if (etch::consume_shortcut(etch::Mod_Primary, SDLK_B, true)) {
        sidebar_open = !sidebar_open;
        config.prefs().sidebar_open = sidebar_open;
    }
    // Primary+N: New Session
    if (etch::consume_shortcut(etch::Mod_Primary, SDLK_N, true)) {
        if (!showcase_mode) client.new_session();
        else { client.messages().clear(); clear_doc_cache(); }
        settings_open = false;
        about_open = false;
    }
    // Primary+,: Settings Center
    if (etch::consume_shortcut(etch::Mod_Primary, SDLK_COMMA, true)) {
        settings_open = !settings_open;
        about_open = false;
        if (settings_open) {
            config.load_custom_providers();
            config.load_omp_config();
            sync_settings_buffers();
        }
    }
    // Alt+W / Alt+A / Alt+T / Alt+B: Workbench tabs (Changes / Agents / Session Tree / /btw)
    if (etch::consume_shortcut(etch::Mod_Alt, SDLK_W, true)) {
        if (inspector_open && inspector_tab == InspectorTab::Changes) inspector_open = false;
        else { inspector_open = true; inspector_tab = InspectorTab::Changes; }
    }
    if (etch::consume_shortcut(etch::Mod_Alt, SDLK_A, true)) {
        if (inspector_open && inspector_tab == InspectorTab::Agents) inspector_open = false;
        else { inspector_open = true; inspector_tab = InspectorTab::Agents; if (!showcase_mode) client.refresh_subagents(); }
    }
    if (etch::consume_shortcut(etch::Mod_Alt, SDLK_T, true)) {
        if (inspector_open && inspector_tab == InspectorTab::Tree) inspector_open = false;
        else { inspector_open = true; inspector_tab = InspectorTab::Tree; if (!showcase_mode) client.refresh_tree(); }
    }
    if (etch::consume_shortcut(etch::Mod_Alt, SDLK_B, true)) {
        if (inspector_open && inspector_tab == InspectorTab::Btw) inspector_open = false;
        else { inspector_open = true; inspector_tab = InspectorTab::Btw; }
    }
    // Primary+V while composer focused: check clipboard image attachment!
    if (etch::focused(etch::id("composer_input")) && etch::shortcut(etch::Mod_Primary, SDLK_V, true)) {
        auto img = etch::get_clipboard_image();
        if (img.bytes && img.size_bytes > 0) {
            ImageAttachment att;
            att.mime_type = img.mime_type.length > 0 ? std::string(img.mime_type.chars, img.mime_type.length) : "image/png";
            att.base64_data = base64_encode(img.bytes, img.size_bytes);
            att.filename = "clipboard-" + std::to_string(pending_images.size() + 1) + ".png";
            pending_images.push_back(std::move(att));
            client.push_toast(t("toast.image_attached", "Image attached from clipboard"), "info", 2200);
        }
    }
    // Escape cascade: close modals/popovers first, else abort running turn with queue restore + dissolve!
    if (etch::consume_key_pressed(SDLK_ESCAPE)) {
        if (cmd_palette_open) { cmd_palette_open = false; }
        else if (model_popover_open) { model_popover_open = false; }
        else if (thinking_popover_open) { thinking_popover_open = false; }
        else if (workspace_popover_open) { workspace_popover_open = false; }
        else if (about_open) { about_open = false; }
        else if (settings_open) { settings_open = false; }
        else if (renaming_session) { renaming_session = false; }
        else if (client.state().is_streaming) {
            const etch::Rect cr = etch::element_bounds(etch::id("composer_card"));
            if (cr.w > 10.0f) {
                particles.spawn_dissolve_rect(cr, palette().danger, palette().accent, DissolveStyle::Ember, 54);
            }
            if (!showcase_mode) client.abort_and_restore_queue();
        }
    }
}

void App::handle_file_dialogs_and_drops() {
    auto res = etch::poll_file_dialog(ctx);
    if (res.ready && !res.cancelled && !res.error && !res.paths.empty()) {
        const std::string first(res.paths[0].chars, res.paths[0].length);
        if (res.request_tag == kDialogOpenWorkspace) {
            start_omp_for_workspace(first);
            config.scan_sessions();
            workspace_popover_open = false;
            client.push_toast(tf("toast.workspace_switched", "Switched workspace: {name}",
                                 {{"name", fs::path(first).filename().string()}}), "info", 2600);
        } else if (res.request_tag == kDialogPickOmpExe) {
            config.prefs().omp_executable = first;
            config.save_prefs();
            copy_to_buf(omp_path_buf, sizeof(omp_path_buf), first);
            omp_version_label = query_omp_version(first);
            client.push_toast(t("toast.omp_path_updated", "Updated OMP binary path"), "success", 2600);
        } else if (res.request_tag == kDialogAttachFile) {
            for (const auto& sp : res.paths) {
                const std::string path(sp.chars, sp.length);
                const std::string ext = fs::path(path).extension().string();
                if (is_image_ext(ext)) {
                    std::ifstream f(path, std::ios::binary);
                    if (f) {
                        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                        if (!bytes.empty() && bytes.size() <= 16u * 1024u * 1024u) {
                            ImageAttachment att;
                            att.mime_type = mime_for_ext(ext);
                            att.base64_data = base64_encode(bytes.data(), bytes.size());
                            att.filename = fs::path(path).filename().string();
                            pending_images.push_back(std::move(att));
                        }
                    }
                } else {
                    attached_files.push_back(path);
                }
            }
        }
    }

    // Native OS Drag-and-Drop onto window
    for (const auto& sp : etch::dropped_files()) {
        const std::string path(sp.chars, sp.length);
        std::error_code ec;
        if (fs::is_directory(path, ec)) {
            start_omp_for_workspace(path);
            config.scan_sessions();
            client.push_toast(tf("toast.workspace_switched", "Switched workspace: {name}",
                                 {{"name", fs::path(path).filename().string()}}), "info", 2800);
        } else if (is_image_ext(fs::path(path).extension().string())) {
            std::ifstream f(path, std::ios::binary);
            if (f) {
                std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                if (!bytes.empty() && bytes.size() <= 16u * 1024u * 1024u) {
                    ImageAttachment att;
                    att.mime_type = mime_for_ext(fs::path(path).extension().string());
                    att.base64_data = base64_encode(bytes.data(), bytes.size());
                    att.filename = fs::path(path).filename().string();
                    pending_images.push_back(std::move(att));
                    particles.spawn_point_spark(etch::pointer_pos(), palette().accent, 20);
                }
            }
        } else {
            attached_files.push_back(path);
            particles.spawn_point_spark(etch::pointer_pos(), palette().secondary, 16);
        }
    }
}

void App::build_frame() {
    handle_global_shortcuts();
    handle_file_dialogs_and_drops();

    const Palette& pal = palette();
    const etch::Color root_bg = config.prefs().mica_backdrop && !headless_mode
        ? etch::Color::rgba8(pal.bg_canvas.r, pal.bg_canvas.g, pal.bg_canvas.b, 224)
        : pal.bg_canvas;

    ETCH_BOX({
        .id = etch::id("app_root"),
        .axis = etch::Axis::Row,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::grow(),
        .background = root_bg,
        .window_drag_passthrough = true,
    }) {
        // Left collapsible navigation rail (spring animated width)
        build_sidebar(*this);

        // Main workspace column: disappearing titlebar + editorial stage + contextual inspector
        ETCH_BOX({
            .id = etch::id("workspace_col"),
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::grow(),
            .background = root_bg,
            .shrink_width = true,
            .window_drag_passthrough = true,
        }) {
            build_titlebar(*this);

            // Main center stage + Resizable Split-Pane Workbench (using upgraded Etch SecondFixedPx split!)
            if (inspector_open) {
                etch::begin_split({
                    .id = etch::id("workbench_split"),
                    .axis = etch::Axis::Row,
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::grow(),
                    .min_first_px = 440.0f,
                    .min_second_px = 310.0f,
                    .divider_size_px = 1.0f,
                    .divider_color = pal.border_subtle,
                    .divider_hover_color = pal.accent_border,
                    .divider_active_color = pal.accent,
                    .divider_hit_px = 8.0f,
                    .sizing = etch::SplitSizing::SecondFixedPx,
                    .extent_px = &workbench_width_px,
                    .default_extent_px = 412.0f,
                    .show_grip = false,
                    .hover_highlight_px = 2.5f,
                });
                ETCH_BOX({
                    .id = etch::id("center_stage"),
                    .axis = etch::Axis::Col,
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::grow(),
                    .shrink_width = true,
                    .shrink_height = true,
                }) {
                    build_chat_view(*this);
                    build_composer(*this);
                }
                etch::split_second();
                build_inspector(*this);
                etch::end_split();
            } else {
                ETCH_BOX({
                    .id = etch::id("center_stage"),
                    .axis = etch::Axis::Col,
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::grow(),
                    .shrink_width = true,
                    .shrink_height = true,
                }) {
                    build_chat_view(*this);
                    build_composer(*this);
                }
            }
        }

        // Focused product overlays (Settings, About, Command Palette, Popovers, Toasts)
        build_overlays(*this);
        if (settings_open) build_settings_view(*this);
        if (about_open) build_about_modal(*this);

        // Topmost GPU-batched dissolve & micro-interaction particle layer (1 draw call, 0 when idle)
        particles.render(pal.dark);
    }
}

} // namespace ompe
