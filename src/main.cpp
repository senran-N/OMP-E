#include "ui/app.h"
#include "core/platform.h"
#include "omp/mock_replay.h"
#include "theme/app_icon_rgba.h"

#include <SDL3/SDL.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

etch::Vec2 window_logical_size(SDL_Window* win) noexcept {
    if (!win) return {1360.0f, 860.0f};
    int w = 0, h = 0;
    SDL_GetWindowSize(win, &w, &h);
    const float display_scale = SDL_GetWindowDisplayScale(win);
    const float density = SDL_GetWindowPixelDensity(win);
    const float safe_display = (display_scale > 0.05f) ? display_scale : 1.0f;
    const float safe_density = (density > 0.25f) ? density : 1.0f;
    const float coord_scale = safe_display / safe_density;
    return {
        (coord_scale > 0.05f) ? (static_cast<float>(w) / coord_scale) : static_cast<float>(w),
        (coord_scale > 0.05f) ? (static_cast<float>(h) / coord_scale) : static_cast<float>(h),
    };
}

void execute_pending_window_action(etch::Context* ctx, SDL_Window* win, ompe::App* app) noexcept {
    if (!app || !win) return;
    const auto act = app->pending_win_action;
    app->pending_win_action = ompe::App::PendingWindowAction::None;
    switch (act) {
        case ompe::App::PendingWindowAction::Minimize:
            etch::minimize_window(win);
            break;
        case ompe::App::PendingWindowAction::ToggleMaximize:
            if (etch::is_window_fullscreen(ctx, win)) {
                etch::set_window_fullscreen(false, win);
            } else if (etch::is_window_maximized(ctx, win)) {
                etch::restore_window(win);
            } else {
                etch::maximize_window(win);
            }
            break;
        case ompe::App::PendingWindowAction::ToggleFullscreen:
            etch::set_window_fullscreen(!etch::is_window_fullscreen(ctx, win), win);
            break;
        case ompe::App::PendingWindowAction::None:
        default:
            break;
    }
}

void on_window_refresh(etch::Context* ctx, SDL_Window* win, float dt, void* user_data) noexcept {
    auto* app = static_cast<ompe::App*>(user_data);
    if (!app || !win) return;
    if (etch::is_window_minimized(ctx, win)) return;

    app->viewport = window_logical_size(win);
    app->tick(dt > 0.0f && dt < 0.05f ? dt : (1.0f / 60.0f));
    etch::begin_window_frame(ctx, win, dt);
    app->build_frame();
    etch::end_window_frame(ctx);

    if (app->pending_font_reload) {
        app->reload_fonts();
    }

    const bool will_minimize = (app->pending_win_action == ompe::App::PendingWindowAction::Minimize);
    execute_pending_window_action(ctx, win, app);
    if (!will_minimize && !etch::is_window_minimized(ctx, win)) {
        etch::render_window(ctx, app->palette().bg_canvas, win);
    }
}

int run_headless_suite(const std::string& out_dir) {
    std::error_code ec;
    fs::create_directories(out_dir, ec);

    etch::Context* ctx = etch::create_context(etch::ContextDesc{
        .headless = true,
    });
    if (!ctx) {
        std::fprintf(stderr, "Headless create_context failed: %s\n", SDL_GetError());
        return 1;
    }

    ompe::App app;
    app.init(ctx, nullptr, true, true);
    app.viewport = {1440.0f, 900.0f};

    auto render_scene = [&](const char* filename, int settle_frames = 28) -> bool {
        for (int f = 0; f < settle_frames; ++f) {
            app.tick(1.0f / 60.0f);
            etch::begin_frame(ctx, etch::FrameDesc{
                .viewport_w = 1440u,
                .viewport_h = 900u,
                .dpi_scale = 1.0f,
                .dt_seconds = 1.0f / 60.0f,
            });
            app.build_frame();
            etch::end_frame(ctx);
        }
        const std::string full = (fs::path(out_dir) / filename).string();
        const bool ok = etch::render_headless_png(ctx, 1440u, 900u, full.c_str(), app.palette().bg_canvas);
        const auto stats = etch::frame_stats(ctx);
        std::printf("[headless] %-26s ok=%d  prims=%u  draws=%u\n",
                    filename, ok ? 1 : 0, stats.prim_count, stats.draw_call_count);
        std::fflush(stdout);
        return ok && stats.draw_call_count > 0;
    };

    bool all_ok = true;

    // Scene 1: Obsidian Brass (en-US) — Zero-Card Exploration Trace + Plan Card + Diff Ledger + Bash Terminal
    app.i18n.set_locale("en-US");
    app.config.prefs().language = "en-US";
    app.config.prefs().palette = ompe::PaletteId::ObsidianBrass;
    app.apply_theme();
    app.sidebar_open = true;
    app.inspector_open = false;
    app.settings_open = false;
    app.about_open = false;
    all_ok &= render_scene("01_chat_obsidian.png");

    // Scene 2: Split-Pane Workbench -> Changes (Structured OMP Diff w/ line numbers + word emphasis + Thinking Effort Popover!)
    app.inspector_open = true;
    app.inspector_tab = ompe::InspectorTab::Changes;
    app.focused_workbench_tool_id = "call_edit_1";
    app.thinking_popover_open = true;
    app.expanded_disclosures.insert("exp_msg_a1_1_1"); // Show expanded borderless Exploration Trace timeline!
    app.particles.spawn_send_burst({380.0f, 760.0f, 440.0f, 4.0f}, app.palette().accent, app.palette().secondary);
    all_ok &= render_scene("02_workbench_changes.png", 16);
    app.thinking_popover_open = false;
    app.expanded_disclosures.erase("exp_msg_a1_1_1");

    // Scene 3: Split-Pane Workbench -> Agents Hub
    app.particles.clear();
    app.inspector_tab = ompe::InspectorTab::Agents;
    all_ok &= render_scene("03_agents_hub.png", 16);

    // Scene 4: Split-Pane Workbench -> Session Tree DAG Canvas
    app.inspector_tab = ompe::InspectorTab::Tree;
    all_ok &= render_scene("04_tree_canvas.png", 16);

    // Scene 5: Settings Center -> General (priority #1 tab: Track Slider, shadowed Switches, 2-group CORE/SYSTEM nav)
    app.inspector_open = false;
    app.settings_open = true;
    app.settings_tab = ompe::SettingsTab::General;
    all_ok &= render_scene("05_settings_general.png", 16);

    // Scene 6: Settings Center -> Providers (bordered/shadowed ComboBox & 1-click presets)
    app.settings_tab = ompe::SettingsTab::Providers;
    app.provider_form.open = true;
    all_ok &= render_scene("06_settings_providers.png", 16);

    // Scene 7: Welcome Stage with Crystalline Particle Dust Shader (Obsidian Brass)
    app.settings_open = false;
    app.client.messages().clear();
    app.client.state().steering_queue.clear();
    app.client.state().follow_up_queue.clear();
    app.clear_doc_cache();
    all_ok &= render_scene("07_welcome_obsidian.png", 18);

    // Scene 8: Signature Light Mode ("Alabaster Ink") + About Modal
    app.config.prefs().palette = ompe::PaletteId::AlabasterInk;
    app.apply_theme();
    app.about_open = true;
    all_ok &= render_scene("08_alabaster_about.png", 18);

    // Scene 9: Light Mode ("Alabaster Ink") Active Chat (Editorial Paper Reading Flow + Crisp Inline Code + Dark Terminal)
    app.about_open = false;
    ompe::populate_showcase_fixture(app.client);
    app.clear_doc_cache();
    all_ok &= render_scene("09_chat_alabaster.png", 20);

    // Scene 10: Celadon Slate Palette + Appearance Settings (Architectural Theme Preview Cards + Dissolve Playground)
    app.config.prefs().palette = ompe::PaletteId::CeladonSlate;
    app.apply_theme();
    app.settings_open = true;
    app.settings_tab = ompe::SettingsTab::Appearance;
    all_ok &= render_scene("10_appearance_celadon.png", 18);

    // Scene 11: Settings Center -> Models (Default Thinking Effort 4-bar Segmented Selector + Roles)
    app.config.prefs().palette = ompe::PaletteId::ObsidianBrass;
    app.apply_theme();
    app.settings_tab = ompe::SettingsTab::Models;
    all_ok &= render_scene("11_settings_models.png", 16);

    // Scene 12: Simplified Chinese (zh-CN) Locale Pack + Redesigned Branch Tree Showcase
    app.i18n.set_locale("zh-CN");
    app.config.prefs().language = "zh-CN";
    app.settings_open = false;
    app.inspector_open = true;
    app.inspector_tab = ompe::InspectorTab::Tree;
    all_ok &= render_scene("12_chat_tree_zh.png", 16);

    // Scene 13: Simplified Chinese (zh-CN) Settings General (Dynamic Locale Picker + Typography)
    app.inspector_open = false;
    app.settings_open = true;
    app.settings_tab = ompe::SettingsTab::General;
    all_ok &= render_scene("13_settings_zh.png", 16);

    app.shutdown();
    etch::destroy_context(ctx);
    return all_ok ? 0 : 1;
}

int run_live_rpc_smoke() {
    ompe::OmpClient client;
    ompe::ClientLaunchConfig cfg;
    cfg.omp_executable = ompe::find_omp_executable();
    cfg.working_dir = ompe::current_working_dir();
    if (!client.connect(cfg)) {
        std::fprintf(stderr, "[smoke-rpc] Failed to spawn OMP: %s\n", client.status_error().c_str());
        return 1;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    bool got_v2 = false;
    while (std::chrono::steady_clock::now() < deadline) {
        client.poll();
        if (client.status() == ompe::ConnectionStatus::Error) {
            std::fprintf(stderr, "[smoke-rpc] Error: %s\n", client.status_error().c_str());
            client.disconnect();
            return 1;
        }
        if (client.negotiated_version() == 2 && !client.state().session_id.empty() &&
            !client.login_providers().empty() && !client.available_models().empty()) {
            got_v2 = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    std::printf("[smoke-rpc] status=%d  proto=v%d  session=%s  model=%s  models=%zu  login_providers=%zu  commands=%zu\n",
                static_cast<int>(client.status()),
                client.negotiated_version(),
                client.state().session_id.c_str(),
                client.state().model.selector().c_str(),
                client.available_models().size(),
                client.login_providers().size(),
                client.available_commands().size());
    client.disconnect();
    return (got_v2 || (client.negotiated_version() == 2 && !client.state().session_id.empty() && !client.login_providers().empty())) ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    bool headless = false;
    bool showcase = false;
    bool smoke_rpc = false;
    std::string out_dir = ".ci/screens";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--headless") == 0) {
            headless = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                out_dir = argv[++i];
            }
        } else if (std::strcmp(argv[i], "--showcase") == 0) {
            showcase = true;
        } else if (std::strcmp(argv[i], "--smoke-rpc") == 0) {
            smoke_rpc = true;
        }
    }

#ifdef _WIN32
    // GUI subsystem binary: attach to parent terminal ONLY for CLI diagnostic modes when stdout isn't piped
    if (headless || smoke_rpc) {
        const HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!h_out || h_out == INVALID_HANDLE_VALUE || GetFileType(h_out) == FILE_TYPE_UNKNOWN) {
            if (AttachConsole(ATTACH_PARENT_PROCESS)) {
                std::freopen("CONOUT$", "w", stdout);
                std::freopen("CONOUT$", "w", stderr);
            }
        }
    }
#endif

    try {
        if (smoke_rpc) {
            return run_live_rpc_smoke();
        }
        if (headless) {
            return run_headless_suite(out_dir);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[fatal] Uncaught std::exception: %s\n", e.what());
        return 2;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    ompe::ConfigStore boot_cfg;
    // Sanitize legacy physical-pixel inflated sizes so restored windows leave clear room for Maximize/Restore
    float init_w = static_cast<float>(boot_cfg.prefs().window_w);
    float init_h = static_cast<float>(boot_cfg.prefs().window_h);
    if (const SDL_DisplayID prim = SDL_GetPrimaryDisplay()) {
        SDL_Rect usable{};
        const float dscale = std::max(1.0f, SDL_GetDisplayContentScale(prim));
        if (SDL_GetDisplayUsableBounds(prim, &usable) && usable.w > 0 && usable.h > 0) {
            const float max_logical_w = static_cast<float>(usable.w) / dscale;
            const float max_logical_h = static_cast<float>(usable.h) / dscale;
            if (init_w >= max_logical_w * 0.94f && init_h >= max_logical_h * 0.94f) {
                init_w = std::clamp(max_logical_w * 0.84f, 960.0f, 1380.0f);
                init_h = std::clamp(max_logical_h * 0.84f, 640.0f, 860.0f);
                boot_cfg.prefs().window_w = static_cast<int>(init_w);
                boot_cfg.prefs().window_h = static_cast<int>(init_h);
                boot_cfg.save_prefs();
            }
        }
    }

    uint64_t win_flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef _WIN32
    if (boot_cfg.prefs().mica_backdrop) {
        win_flags |= SDL_WINDOW_TRANSPARENT;
    }
#endif

    SDL_Window* window = etch::create_desktop_window(
        "OMP Desktop",
        init_w,
        init_h,
        win_flags);
    if (!window) {
        std::fprintf(stderr, "create_desktop_window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    etch::set_desktop_window_min_size(window, 880.0f, 600.0f);
    if (SDL_Surface* icon_surf = SDL_CreateSurfaceFrom(
            ompe::kAppIconSize, ompe::kAppIconSize, SDL_PIXELFORMAT_RGBA32,
            const_cast<uint8_t*>(ompe::kAppIconRgba), ompe::kAppIconSize * 4)) {
        SDL_SetWindowIcon(window, icon_surf);
        SDL_DestroySurface(icon_surf);
    }

    etch::Context* ctx = etch::create_context(etch::ContextDesc{
        .window = window,
        .frame_arena_bytes = 16u * 1024u * 1024u,
        .max_prims_per_frame = 32768u,
    });
    if (!ctx) {
        std::fprintf(stderr, "etch::create_context failed\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    ompe::App app;
    app.init(ctx, window, false, showcase);
    etch::set_window_refresh_callback(ctx, on_window_refresh, &app);

    // Paint initial frame immediately
    on_window_refresh(ctx, window, 1.0f / 60.0f, &app);

    Uint64 prev_ns = SDL_GetTicksNS();
    while (app.running) {
        SDL_Event ev;
        if (etch::wait_events(ctx, &ev)) {
            do {
                etch::process_event(ctx, ev);
                if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                    app.running = false;
                } else if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ev.key.key == SDLK_F11) {
                    etch::set_window_fullscreen(!etch::is_window_fullscreen(ctx, window), window);
                } else if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ev.key.key == SDLK_F12) {
                    etch::set_debug_hud_open(ctx, !etch::debug_hud_open(ctx));
                }
            } while (SDL_PollEvent(&ev));
        }
        if (!app.running) break;

        const bool rpc_changed = !app.showcase_mode && app.client.poll();
        if (rpc_changed || etch::needs_redraw(ctx)) {
            const Uint64 now_ns = SDL_GetTicksNS();
            float dt = static_cast<float>(now_ns - prev_ns) * 1e-9f;
            prev_ns = now_ns;
            if (dt <= 0.0f || dt > 0.05f) dt = 1.0f / 60.0f;

            const etch::Vec2 logical_sz = window_logical_size(window);
            if (!etch::is_window_maximized(ctx, window) &&
                !etch::is_window_minimized(ctx, window) &&
                !etch::is_window_fullscreen(ctx, window) &&
                logical_sz.x >= 880.0f && logical_sz.y >= 600.0f) {
                app.config.prefs().window_w = static_cast<int>(std::round(logical_sz.x));
                app.config.prefs().window_h = static_cast<int>(std::round(logical_sz.y));
            }
            on_window_refresh(ctx, window, dt, &app);
        } else {
            prev_ns = SDL_GetTicksNS();
        }
    }

    etch::set_window_refresh_callback(ctx, nullptr, nullptr);
    app.shutdown();
    etch::destroy_context(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
