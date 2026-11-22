#include "ui/app.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace ompe {
namespace {

bool icontains(const std::string& hay, const char* needle) {
    if (!needle || needle[0] == '\0') return true;
    const size_t nlen = std::strlen(needle);
    if (nlen > hay.size()) return false;
    for (size_t i = 0; i + nlen <= hay.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < nlen; ++j) {
            if (std::tolower(static_cast<unsigned char>(hay[i + j])) !=
                std::tolower(static_cast<unsigned char>(needle[j]))) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

std::string short_folder_name(const std::string& path, const char* fallback) {
    if (path.empty()) return fallback;
    fs::path p(path);
    std::string fn = p.filename().string();
    return fn.empty() ? path : fn;
}

} // namespace

void build_sidebar(App& app) {
    const Palette& pal = app.palette();
    const TypeScale ts = app.type();
    const float w = etch::spring(etch::id("sidebar_width_spring"),
                                 app.sidebar_open ? kSidebarW : 0.0f,
                                 app.sidebar_open ? kSidebarW : 0.0f,
                                 kMotionFluid);
    if (w < 4.0f) return;

    const etch::Padding safe = etch::window_safe_insets(app.ctx, app.window);
    const float top_pad = safe.left > 0.0f ? 36.0f : 10.0f;
    const float brand_h = kTitlebarH + (top_pad > 10.0f ? 12.0f : 0.0f);
    const uint32_t brand_id = etch::id("sb_brand_bar");
    const etch::Rect brand_b = etch::element_bounds(brand_id);
    if (brand_b.w > 20.0f && brand_b.h > 8.0f) {
        etch::window_drag_region(brand_b);
    } else {
        etch::window_drag_region({0.0f, 0.0f, w, brand_h});
    }

    ETCH_BOX({
        .id = etch::id("sidebar_rail"),
        .axis = etch::Axis::Col,
        .width = etch::Sizing::fixed(w),
        .height = etch::Sizing::grow(),
        .background = pal.bg_sidebar,
        .border_color = pal.border_subtle,
        .clip_children = true,
        .window_drag_passthrough = true,
        .border_sides = {.left = 0.0f, .right = 1.0f, .top = 0.0f, .bottom = 0.0f},
    }) {
        // Editorial Serif Brand Header & state-reactive sidebar collapse bar (frameless, zero generic square box)
        ETCH_BOX({
            .id = brand_id,
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fixed(brand_h),
            .padding = {.left = 14.0f, .right = 10.0f, .top = top_pad > 10.0f ? 22.0f : 0.0f, .bottom = 0.0f},
            .gap = 8.0f,
            .align_y = etch::AlignY::Center,
            .window_drag_passthrough = true,
        }) {
            etch::icon({.icon_id = app.icons.logo, .size_px = 17.0f, .color = pal.accent});
            etch::text(etch::str_lit("OMP"), {
                .color = pal.text_primary,
                .font_size_px = ts.h1 + 3.0f,
                .font_id = app.fonts.serif,
                .letter_spacing = 1.1f,
                .wrap = false,
            });

            etch::box({.width = etch::Sizing::grow(), .height = etch::Sizing::grow(), .window_drag_passthrough = true});

            const uint32_t col_id = etch::id("sb_collapse");
            const bool col_hov = etch::hovered(col_id);
            ETCH_BOX({
                .id = col_id,
                .width = etch::Sizing::fixed(26.0f),
                .height = etch::Sizing::fixed(26.0f),
                .align_x = etch::AlignX::Center,
                .align_y = etch::AlignY::Center,
                .background = col_hov ? pal.hover : etch::Color::transparent(),
                .radius = etch::Corners::all(kRadiusSm),
                .continuous_corners = true,
                .press_scale = 0.94f,
                .transition = kMotionSnap,
            }) {
                etch::icon({
                    .icon_id = app.icons.panel_left_close,
                    .size_px = 14.5f,
                    .color = col_hov ? pal.text_primary : pal.text_muted,
                });
            }
            if (col_hov) etch::set_cursor(etch::Cursor::Hand);
            etch::set_tooltip(col_id, app.ts("sidebar.hide", "Hide sidebar (Ctrl+B)"));
            if (etch::clicked(col_id)) {
                app.sidebar_open = false;
                app.config.prefs().sidebar_open = false;
            }
        }
        if (!app.headless_mode && etch::double_clicked(brand_id)) {
            app.pending_win_action = App::PendingWindowAction::ToggleMaximize;
        }

        // Workspace selector + Restrained New Thread trigger (Linear-caliber neutral craft)
        ETCH_BOX({
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .padding = {.left = 10.0f, .right = 10.0f, .top = 6.0f, .bottom = 6.0f},
            .gap = 6.0f,
            .align_y = etch::AlignY::Center,
        }) {
            const uint32_t ws_id = etch::id("sb_workspace_btn");
            const bool ws_hov = etch::hovered(ws_id);
            const std::string ws_name = short_folder_name(app.config.prefs().active_workspace, app.t("common.workspace", "Workspace"));
            const float ws_chev_rad = etch::spring_angle(etch::id("sb_ws_chev"),
                                                         app.workspace_popover_open ? 1.5707963f : 0.0f,
                                                         kMotionSnap);
            ETCH_BOX({
                .id = ws_id,
                .axis = etch::Axis::Row,
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::fixed(ts.row_h),
                .padding = etch::Padding::xy(9.0f, 0.0f),
                .gap = 7.0f,
                .align_y = etch::AlignY::Center,
                .background = (ws_hov || app.workspace_popover_open) ? pal.bg_elevated : pal.bg_surface,
                .border_color = app.workspace_popover_open ? pal.border_strong : pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusSm),
                .shrink_width = true,
                .continuous_corners = true,
            }) {
                etch::icon({
                    .icon_id = app.workspace_popover_open ? app.icons.folder_open : app.icons.folder,
                    .size_px = 14.0f,
                    .color = app.workspace_popover_open ? pal.accent : pal.text_muted,
                });
                ETCH_BOX({.width = etch::Sizing::grow(), .clip_children = true, .shrink_width = true}) {
                    etch::text(etch::str_cstr(ws_name.c_str()), {
                        .color = pal.text_primary,
                        .font_size_px = ts.ui,
                        .font_id = app.fonts.medium,
                        .wrap = false,
                        .ellipsis = true,
                    });
                }
                etch::icon({
                    .icon_id = app.icons.chevron_r,
                    .size_px = 11.0f,
                    .color = pal.text_faint,
                    .rotation_rad = ws_chev_rad,
                });
            }
            if (ws_hov) etch::set_cursor(etch::Cursor::Hand);
            etch::set_tooltip(ws_id, etch::str_cstr(app.config.prefs().active_workspace.c_str()));
            if (etch::clicked(ws_id)) {
                app.workspace_popover_open = !app.workspace_popover_open;
                app.model_popover_open = false;
            }

            // Quiet neutral New Thread button with dedicated square-pen compose glyph
            const uint32_t new_id = etch::id("sb_new_thread");
            const bool new_hov = etch::hovered(new_id);
            ETCH_BOX({
                .id = new_id,
                .width = etch::Sizing::fixed(ts.row_h),
                .height = etch::Sizing::fixed(ts.row_h),
                .align_x = etch::AlignX::Center,
                .align_y = etch::AlignY::Center,
                .background = new_hov ? pal.bg_elevated : pal.bg_surface,
                .border_color = new_hov ? pal.accent_border : pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusSm),
                .continuous_corners = true,
                .press_scale = 0.93f,
                .transition = kMotionSnap,
            }) {
                etch::icon({
                    .icon_id = app.icons.square_pen,
                    .size_px = 14.5f,
                    .color = new_hov ? pal.accent : pal.text_secondary,
                });
            }
            if (new_hov) etch::set_cursor(etch::Cursor::Hand);
            etch::set_tooltip(new_id, app.ts("sidebar.new_thread", "New thread (Ctrl+N)"));
            if (etch::clicked(new_id)) {
                const etch::Rect r = etch::element_bounds(new_id);
                app.particles.spawn_point_spark({r.x + r.w * 0.5f, r.y + r.h * 0.5f}, pal.accent, 14);
                app.settings_open = false;
                app.about_open = false;
                if (app.showcase_mode) {
                    app.client.messages().clear();
                    app.clear_doc_cache();
                } else {
                    app.client.new_session();
                }
            }
        }

        // Native leading-icon search input (upgraded Etch TextInput: zero focus glow clutter)
        ETCH_BOX({
            .width = etch::Sizing::grow(),
            .padding = {.left = 10.0f, .right = 10.0f, .top = 2.0f, .bottom = 8.0f},
        }) {
            etch::text_input({
                .id = etch::id("sb_search"),
                .buffer = app.sidebar_search,
                .capacity = sizeof(app.sidebar_search),
                .placeholder = app.ts("sidebar.filter_threads", "Filter threads..."),
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::fixed(ts.row_h - 2.0f),
                .padding = etch::Padding::xy(8.0f, 4.0f),
                .font_size_px = ts.ui_sm,
                .font_id = app.fonts.body,
                .text_color = pal.text_primary,
                .placeholder_color = pal.text_faint,
                .background = pal.bg_input,
                .border_color = pal.border_subtle,
                .focus_border_color = pal.border_strong,
                .border_width = 1.0f,
                .focus_border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusSm),
                .clear_button = true,
                .hover_border_color = pal.border_default,
                .focus_glow_blur = 0.0f,
                .leading_icon_id = app.icons.search,
                .leading_icon_color = pal.text_faint,
                .continuous_corners = true,
            });
        }

        // Scope micro-header (Project vs All)
        ETCH_BOX({
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .padding = {.left = 14.0f, .right = 12.0f, .top = 2.0f, .bottom = 4.0f},
            .align_y = etch::AlignY::Center,
        }) {
            etch::text(app.ts("sidebar.threads", "THREADS"), {
                .color = pal.text_faint,
                .font_size_px = ts.micro,
                .font_id = app.fonts.semibold,
                .letter_spacing = app.is_cjk_locale() ? 0.3f : 0.9f,
                .wrap = false,
            });
            etch::box({.width = etch::Sizing::grow()});
            const uint32_t scope_id = etch::id("sb_scope_toggle");
            const bool sc_hov = etch::hovered(scope_id);
            ETCH_BOX({
                .id = scope_id,
                .padding = etch::Padding::xy(6.0f, 1.5f),
                .background = sc_hov ? pal.hover : etch::Color::transparent(),
                .radius = etch::Corners::all(4.0f),
            }) {
                etch::text(app.all_workspaces_filter ? app.ts("sidebar.scope_all", "All") : app.ts("sidebar.scope_project", "Project"), {
                    .color = sc_hov ? pal.text_secondary : pal.text_muted,
                    .font_size_px = ts.meta,
                    .font_id = app.fonts.medium,
                    .wrap = false,
                });
            }
            if (sc_hov) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(scope_id)) app.all_workspaces_filter = !app.all_workspaces_filter;
        }

        // Scrollable flat thread list — Linear/Zed restraint:
        //   - NO clunky left accent bar
        //   - NO loud colored active block
        //   - NO rainbow status dots on every row
        //   - Clean neutral luminance surface elevation + crisp typography + hover-revealed pin/dissolve
        const auto& all_sessions = app.showcase_mode ? app.showcase_sessions : app.config.sessions();
        std::vector<const SessionListItem*> visible;
        visible.reserve(all_sessions.size());
        const auto ws_sessions = app.all_workspaces_filter
            ? std::vector<SessionListItem>{}
            : app.config.sessions_for_workspace(app.config.prefs().active_workspace);

        const std::vector<SessionListItem>& source =
            (app.showcase_mode || app.all_workspaces_filter) ? all_sessions : ws_sessions;

        for (const auto& s : source) {
            if (icontains(s.title, app.sidebar_search) || icontains(s.first_message, app.sidebar_search)) {
                visible.push_back(&s);
            }
        }

        ETCH_BOX({
            .id = etch::id("sb_thread_scroll"),
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::grow(),
            .padding = {.left = 8.0f, .right = 8.0f, .top = 2.0f, .bottom = 8.0f},
            .gap = 1.5f,
            .clip_children = true,
            .scroll_y = true,
            .shrink_height = true,
            .scroll_fade_px = 18.0f,
        }) {
            if (visible.empty()) {
                ETCH_BOX({
                    .width = etch::Sizing::grow(),
                    .padding = etch::Padding::xy(10.0f, 18.0f),
                }) {
                    etch::text(app.ts("sidebar.no_threads", "No threads yet"), {
                        .color = pal.text_faint,
                        .font_size_px = ts.ui_sm,
                        .font_id = app.fonts.body,
                        .wrap = false,
                    });
                }
            }

            std::string pending_delete_path;
            etch::Rect pending_delete_rect{};
            std::string pending_pin_id;

            for (uint32_t idx = 0; idx < visible.size(); ++idx) {
                const SessionListItem& s = *visible[idx];
                const bool is_active = (!app.client.state().session_id.empty() &&
                                        s.session_id == app.client.state().session_id);
                const bool is_streaming_here = is_active && app.client.state().is_streaming;
                const bool held = s.held_externally || (app.client.held_leases().count(s.session_id) > 0 && !is_active);
                const uint32_t row_id = etch::id_index(etch::id("sb_row"), idx);
                const uint32_t pin_id = etch::id_index(etch::id("sb_pin"), idx);
                const uint32_t del_id = etch::id_index(etch::id("sb_del"), idx);
                const bool row_hov = etch::hovered(row_id) || etch::hovered(pin_id) || etch::hovered(del_id);

                const etch::Color row_bg = is_active
                    ? (pal.dark ? etch::Color::rgba8(244, 238, 226, 12) : etch::Color::rgba8(28, 25, 21, 11))
                    : (row_hov ? pal.hover : etch::Color::transparent());

                ETCH_BOX({
                    .id = row_id,
                    .axis = etch::Axis::Row,
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::fixed(ts.row_h),
                    .padding = etch::Padding::xy(9.0f, 0.0f),
                    .gap = 7.0f,
                    .align_y = etch::AlignY::Center,
                    .background = row_bg,
                    .border_color = is_active ? pal.border_subtle : etch::Color::transparent(),
                    .border_width = is_active ? 1.0f : 0.0f,
                    .radius = etch::Corners::all(kRadiusSm),
                    .shrink_width = true,
                    .continuous_corners = true,
                }) {
                    // Only show a leading glyph when it communicates genuine live state (streaming or pinned)
                    if (is_streaming_here) {
                        etch::icon({
                            .icon_id = app.icons.loader_arc,
                            .size_px = 12.5f,
                            .color = pal.accent,
                            .rotation_rad = app.elapsed_s * 5.2f,
                        });
                    } else if (s.pinned) {
                        etch::icon({
                            .icon_id = app.icons.pin_filled,
                            .size_px = 12.0f,
                            .color = is_active ? pal.accent : pal.text_faint,
                        });
                    }

                    ETCH_BOX({.width = etch::Sizing::grow(), .clip_children = true, .shrink_width = true}) {
                        etch::text(etch::str_cstr(s.title.c_str()), {
                            .color = is_active ? pal.text_primary : (row_hov ? pal.text_primary : pal.text_secondary),
                            .font_size_px = ts.ui,
                            .font_id = is_active ? app.fonts.medium : app.fonts.body,
                            .wrap = false,
                            .ellipsis = true,
                        });
                    }

                    // Hover-revealed micro actions (pin toggle + dissolve delete)
                    if (row_hov && !app.showcase_mode && !s.file_path.empty()) {
                        const bool pin_hov = etch::hovered(pin_id);
                        ETCH_BOX({
                            .id = pin_id,
                            .width = etch::Sizing::fixed(20.0f),
                            .height = etch::Sizing::fixed(20.0f),
                            .align_x = etch::AlignX::Center,
                            .align_y = etch::AlignY::Center,
                            .background = pin_hov ? pal.active : etch::Color::transparent(),
                            .radius = etch::Corners::all(5.0f),
                        }) {
                            etch::icon({
                                .icon_id = s.pinned ? app.icons.pin_filled : app.icons.pin,
                                .size_px = 11.0f,
                                .color = (s.pinned || pin_hov) ? pal.accent : pal.text_muted,
                            });
                        }
                        if (etch::clicked(pin_id)) {
                            pending_pin_id = s.session_id;
                        }

                        const bool del_hov = etch::hovered(del_id);
                        ETCH_BOX({
                            .id = del_id,
                            .width = etch::Sizing::fixed(20.0f),
                            .height = etch::Sizing::fixed(20.0f),
                            .align_x = etch::AlignX::Center,
                            .align_y = etch::AlignY::Center,
                            .background = del_hov ? pal.danger_soft : etch::Color::transparent(),
                            .radius = etch::Corners::all(5.0f),
                        }) {
                            etch::icon({.icon_id = app.icons.trash, .size_px = 11.5f, .color = del_hov ? pal.danger : pal.text_muted});
                        }
                        if (etch::clicked(del_id)) {
                            pending_delete_path = s.file_path;
                            pending_delete_rect = etch::element_bounds(row_id);
                        }
                    } else if (held) {
                        ETCH_BOX({
                            .padding = etch::Padding::xy(5.0f, 1.0f),
                            .background = pal.warning_soft,
                            .radius = etch::Corners::all(4.0f),
                        }) {
                            etch::text(etch::str_lit("TUI"), {
                                .color = pal.warning,
                                .font_size_px = ts.micro,
                                .font_id = app.fonts.mono_tnum,
                                .wrap = false,
                            });
                        }
                    }
                }

                if (row_hov) etch::set_cursor(etch::Cursor::Hand);
                if (etch::clicked(row_id) && !etch::clicked(del_id) && !etch::clicked(pin_id)) {
                    app.settings_open = false;
                    app.about_open = false;
                    if (!app.showcase_mode && !s.file_path.empty()) {
                        app.clear_doc_cache();
                        app.stick_chat_bottom = true;
                        app.client.switch_session(s.file_path);
                    }
                }
            }

            if (!pending_pin_id.empty()) {
                app.config.toggle_pin_session(pending_pin_id);
            }
            if (!pending_delete_path.empty()) {
                app.particles.spawn_dissolve_rect(pending_delete_rect, pal.bg_elevated, pal.accent, DissolveStyle::Dust, 58);
                app.config.delete_session_file(pending_delete_path);
                app.client.push_toast(app.t("toast.thread_dissolved", "Thread dissolved"), "info", 2200);
            }
        }

        // Bottom minimal product footer: Settings + About with state-reactive connection glyph
        ETCH_BOX({
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .padding = etch::Padding::all(8.0f),
            .gap = 2.0f,
            .border_color = pal.border_subtle,
            .border_sides = {.left = 0.0f, .right = 0.0f, .top = 1.0f, .bottom = 0.0f},
        }) {
            const uint32_t set_id = etch::id("sb_settings");
            const bool set_hov = etch::hovered(set_id);
            ETCH_BOX({
                .id = set_id,
                .axis = etch::Axis::Row,
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::fixed(ts.row_h),
                .padding = etch::Padding::xy(9.0f, 0.0f),
                .gap = 8.0f,
                .align_y = etch::AlignY::Center,
                .background = app.settings_open ? pal.active : (set_hov ? pal.hover : etch::Color::transparent()),
                .radius = etch::Corners::all(kRadiusSm),
                .continuous_corners = true,
            }) {
                etch::icon({
                    .icon_id = app.icons.settings,
                    .size_px = 14.5f,
                    .color = app.settings_open ? pal.accent : (set_hov ? pal.text_primary : pal.text_muted),
                });
                etch::text(app.ts("sidebar.settings", "Settings"), {
                    .color = (app.settings_open || set_hov) ? pal.text_primary : pal.text_secondary,
                    .font_size_px = ts.ui,
                    .font_id = app.fonts.medium,
                    .wrap = false,
                });
                etch::box({.width = etch::Sizing::grow()});
                etch::text(etch::str_lit("Ctrl+,"), {
                    .color = pal.text_faint,
                    .font_size_px = ts.meta,
                    .font_id = app.fonts.mono_tnum,
                    .wrap = false,
                });
            }
            if (set_hov) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(set_id)) {
                app.settings_open = !app.settings_open;
                app.about_open = false;
                if (app.settings_open) {
                    app.config.load_custom_providers();
                    app.config.load_omp_config();
                    app.sync_settings_buffers();
                }
            }

            // About + state-reactive engine connection icon
            const uint32_t abt_id = etch::id("sb_about");
            const bool abt_hov = etch::hovered(abt_id);
            const auto st = app.client.status();
            const bool is_connecting = (st == ConnectionStatus::Starting || st == ConnectionStatus::Handshaking);
            const uint16_t conn_ico =
                (st == ConnectionStatus::Connected) ? app.icons.plug_connected :
                is_connecting ? app.icons.loader_arc : app.icons.plug_off;
            const etch::Color conn_col =
                (st == ConnectionStatus::Connected) ? pal.success :
                (st == ConnectionStatus::Error) ? pal.danger : pal.warning;
            ETCH_BOX({
                .id = abt_id,
                .axis = etch::Axis::Row,
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::fixed(ts.row_h - 2.0f),
                .padding = etch::Padding::xy(9.0f, 0.0f),
                .gap = 8.0f,
                .align_y = etch::AlignY::Center,
                .background = app.about_open ? pal.active : (abt_hov ? pal.hover : etch::Color::transparent()),
                .radius = etch::Corners::all(kRadiusSm),
                .continuous_corners = true,
            }) {
                etch::icon({.icon_id = app.icons.info, .size_px = 14.0f, .color = abt_hov ? pal.text_primary : pal.text_muted});
                etch::text(app.ts("sidebar.about", "About OMP"), {
                    .color = abt_hov ? pal.text_secondary : pal.text_muted,
                    .font_size_px = ts.ui_sm,
                    .font_id = app.fonts.body,
                    .wrap = false,
                });
                etch::box({.width = etch::Sizing::grow()});
                const auto upd = app.updater.snapshot();
                if (upd.state == UpdateState::Available || upd.state == UpdateState::ReadyToInstall) {
                    ETCH_BOX({
                        .padding = etch::Padding::xy(6.0f, 1.5f),
                        .background = pal.accent_soft,
                        .border_color = pal.accent_border,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(kRadiusPill),
                    }) {
                        const std::string tag = "v" + upd.latest_version;
                        etch::text(etch::str_cstr(tag.c_str()), {
                            .color = pal.accent,
                            .font_size_px = ts.micro,
                            .font_id = app.fonts.mono_tnum,
                            .wrap = false,
                        });
                    }
                }
                etch::icon({
                    .icon_id = conn_ico,
                    .size_px = 12.0f,
                    .color = conn_col,
                    .rotation_rad = is_connecting ? (app.elapsed_s * 5.0f) : 0.0f,
                    .opacity = (st == ConnectionStatus::Connected) ? 0.85f : 1.0f,
                });
            }
            if (abt_hov) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(abt_id)) {
                app.about_open = !app.about_open;
                app.settings_open = false;
            }
        }
    }
}

} // namespace ompe
