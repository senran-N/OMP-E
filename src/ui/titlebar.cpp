#include "ui/app.h"

#include <cstdio>
#include <cstring>

namespace ompe {
namespace {

bool icon_toggle_btn(uint32_t btn_id, uint16_t idle_icon, uint16_t active_icon, bool active_state,
                     const Palette& pal, const TypeScale& ts, const char* tooltip_text, uint32_t badge_count = 0) {
    const bool hov = etch::hovered(btn_id);
    const etch::Color bg = active_state
        ? (pal.dark ? etch::Color::rgba8(244, 238, 226, 18) : etch::Color::rgba8(28, 25, 21, 14))
        : (hov ? pal.hover : etch::Color::transparent());
    const etch::Color fg = active_state ? pal.accent : (hov ? pal.text_primary : pal.text_muted);

    ETCH_BOX({
        .id = btn_id,
        .axis = etch::Axis::Row,
        .width = badge_count > 0 ? etch::Sizing::fit() : etch::Sizing::fixed(30.0f),
        .height = etch::Sizing::fixed(28.0f),
        .padding = badge_count > 0 ? etch::Padding::xy(9.0f, 0.0f) : etch::Padding::all(0.0f),
        .gap = 5.0f,
        .align_x = etch::AlignX::Center,
        .align_y = etch::AlignY::Center,
        .background = bg,
        .border_color = active_state ? pal.border_default : etch::Color::transparent(),
        .border_width = active_state ? 1.0f : 0.0f,
        .radius = etch::Corners::all(kRadiusSm),
        .continuous_corners = true,
        .press_scale = 0.94f,
        .transition = kMotionSnap,
    }) {
        etch::icon({
            .icon_id = active_state ? active_icon : idle_icon,
            .size_px = 15.0f,
            .color = fg,
        });
        if (badge_count > 0) {
            char b[16];
            std::snprintf(b, sizeof(b), "%u", badge_count);
            etch::text(etch::str_cstr(b), {
                .color = active_state ? pal.accent : pal.text_secondary,
                .font_size_px = ts.meta,
                .font_id = etch::kFontMedium,
                .wrap = false,
            });
        }
    }
    if (tooltip_text) etch::set_tooltip(btn_id, etch::str_cstr(tooltip_text));
    if (hov) etch::set_cursor(etch::Cursor::Hand);
    return etch::clicked(btn_id);
}

void win_chrome_btn(uint32_t btn_id, uint16_t icon_id, bool is_close, const Palette& pal,
                    const char* tooltip_text, bool& out_clicked) {
    const bool hov = etch::hovered(btn_id);
    const bool prs = etch::pressed(btn_id);
    const etch::Color bg = prs
        ? (is_close ? etch::Color::lerp(pal.danger, etch::Color::black(), 0.16f) : pal.active)
        : (hov ? (is_close ? pal.danger : pal.hover) : etch::Color::transparent());
    const etch::Color fg = (hov || prs) ? (is_close ? etch::Color::white() : pal.text_primary) : pal.text_muted;
    ETCH_BOX({
        .id = btn_id,
        .width = etch::Sizing::fixed(38.0f),
        .height = etch::Sizing::fixed(32.0f),
        .align_x = etch::AlignX::Center,
        .align_y = etch::AlignY::Center,
        .background = bg,
        .radius = etch::Corners::all(6.0f),
        .continuous_corners = true,
    }) {
        etch::icon({.icon_id = icon_id, .size_px = 12.0f, .color = fg});
    }
    if (tooltip_text) etch::set_tooltip(btn_id, etch::str_cstr(tooltip_text));
    if (hov) etch::set_cursor(etch::Cursor::Hand);
    out_clicked = etch::clicked(btn_id);
}

} // namespace

void build_titlebar(App& app) {
    const Palette& pal = app.palette();
    const TypeScale ts = app.type();
    const etch::Padding safe = etch::window_safe_insets(app.ctx, app.window);
    const float left_inset = (!app.sidebar_open && safe.left > 0.0f) ? safe.left : 0.0f;

    // Register native OS drag region across the top titlebar (interactive child boxes auto-punch client holes)
    const uint32_t tb_id = etch::id("titlebar");
    const etch::Rect tb_bounds = etch::element_bounds(tb_id);
    if (tb_bounds.w > 20.0f && tb_bounds.h > 8.0f) {
        etch::window_drag_region(tb_bounds);
    } else {
        etch::window_drag_region({0.0f, 0.0f, app.viewport.x, kTitlebarH});
    }

    // Count active subagents & touched files for quiet live counts
    uint32_t active_subagents = 0;
    for (const auto& s : app.client.subagents()) {
        if (s.status == "running" || s.status == "active" || s.status == "pending") ++active_subagents;
    }
    uint32_t mutated_files = 0;
    for (const auto& [_, te] : app.client.tool_executions()) {
        if (te.tool_name == "edit" || te.tool_name == "write" || te.tool_name == "patch") ++mutated_files;
    }

    ETCH_BOX({
        .id = tb_id,
        .axis = etch::Axis::Row,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::fixed(kTitlebarH),
        .padding = {.left = 12.0f + left_inset, .right = 8.0f, .top = 0.0f, .bottom = 0.0f},
        .gap = 6.0f,
        .align_y = etch::AlignY::Center,
        .background = etch::Color::transparent(),
        .border_color = pal.border_subtle,
        .window_drag_passthrough = true,
        .border_sides = {.left = 0.0f, .right = 0.0f, .top = 0.0f, .bottom = 1.0f},
    }) {
        // State-reactive sidebar re-open toggle when collapsed
        if (!app.sidebar_open) {
            if (icon_toggle_btn(etch::id("tb_sidebar"), app.icons.panel_left_open, app.icons.panel_left_close,
                                false, pal, ts, app.t("titlebar.show_sidebar", "Show sidebar (Ctrl+B)"))) {
                app.sidebar_open = true;
                app.config.prefs().sidebar_open = true;
            }
        }

        // Session title (click to inline-rename)
        const std::string title = !app.client.state().session_name.empty()
            ? app.client.state().session_name
            : (app.client.messages().empty()
                   ? app.t("titlebar.new_thread", "New thread")
                   : app.t("titlebar.untitled_thread", "Untitled thread"));

        if (app.renaming_session) {
            auto r = etch::text_input({
                .id = etch::id("tb_rename_input"),
                .buffer = app.rename_buf,
                .capacity = sizeof(app.rename_buf),
                .placeholder = app.ts("titlebar.rename_placeholder", "Thread name..."),
                .width = etch::Sizing::fixed(300.0f),
                .height = etch::Sizing::fixed(ts.pill_h),
                .padding = etch::Padding::xy(8.0f, 3.0f),
                .font_size_px = ts.ui,
                .font_id = app.fonts.medium,
                .background = pal.bg_input,
                .border_color = pal.accent_border,
                .focus_border_color = pal.accent,
                .radius = etch::Corners::all(kRadiusXs),
                .submit_on_enter = true,
                .focus_glow_blur = 0.0f,
            });
            if (r.submitted) {
                std::string next = app.rename_buf;
                if (!next.empty()) {
                    app.client.state().session_name = next;
                    if (!app.showcase_mode) app.client.rename_session(next);
                }
                app.renaming_session = false;
            }
        } else {
            const uint32_t title_id = etch::id("tb_title_btn");
            const bool hov = etch::hovered(title_id);
            ETCH_BOX({
                .id = title_id,
                .axis = etch::Axis::Row,
                .width = etch::Sizing::fit(0.0f, 440.0f),
                .height = etch::Sizing::fixed(ts.pill_h),
                .padding = etch::Padding::xy(8.0f, 0.0f),
                .gap = 8.0f,
                .align_y = etch::AlignY::Center,
                .background = hov ? pal.hover : etch::Color::transparent(),
                .radius = etch::Corners::all(kRadiusSm),
                .shrink_width = true,
                .continuous_corners = true,
            }) {
                // Live rotating arc ONLY when actively streaming or compacting
                if (app.client.state().is_streaming || app.client.state().is_compacting) {
                    etch::icon({
                        .icon_id = app.icons.loader_arc,
                        .size_px = 13.0f,
                        .color = app.client.state().is_compacting ? pal.warning : pal.accent,
                        .rotation_rad = app.elapsed_s * 5.2f,
                    });
                }
                etch::text(etch::str_cstr(title.c_str()), {
                    .color = pal.text_primary,
                    .font_size_px = ts.ui,
                    .font_id = app.fonts.medium,
                    .wrap = false,
                    .ellipsis = true,
                });
            }
            if (hov) etch::set_cursor(etch::Cursor::Hand);
            etch::set_tooltip(title_id, app.ts("titlebar.click_to_rename", "Click to rename thread"));
            if (etch::clicked(title_id)) {
                const size_t n = std::min(sizeof(app.rename_buf) - 1, title.size());
                std::memcpy(app.rename_buf, title.data(), n);
                app.rename_buf[n] = '\0';
                app.renaming_session = true;
                etch::request_focus(etch::id("tb_rename_input"));
            }
        }

        // Draggable flexible spacer (double-click toggles maximize/restore cleanly on all platforms)
        const uint32_t drag_id = etch::id("tb_drag_spacer");
        etch::box({
            .id = drag_id,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::grow(),
            .window_drag_passthrough = true,
        });
        if (!app.headless_mode && (etch::double_clicked(tb_id) || etch::double_clicked(drag_id))) {
            app.pending_win_action = App::PendingWindowAction::ToggleMaximize;
        }

        // Command Palette trigger pill (Ctrl+K) — bespoke ⌘ command glyph (distinct from sidebar search)
        {
            const uint32_t k_id = etch::id("tb_cmd_k");
            const bool hov = etch::hovered(k_id);
            ETCH_BOX({
                .id = k_id,
                .axis = etch::Axis::Row,
                .height = etch::Sizing::fixed(28.0f),
                .padding = etch::Padding::xy(9.0f, 0.0f),
                .gap = 6.0f,
                .align_y = etch::AlignY::Center,
                .background = hov ? pal.hover : pal.bg_surface,
                .border_color = hov ? pal.border_strong : pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusSm),
                .continuous_corners = true,
            }) {
                etch::icon({.icon_id = app.icons.command, .size_px = 12.5f, .color = pal.text_muted});
#ifdef __APPLE__
                etch::text(etch::str_lit("⌘K"), {.color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false});
#else
                etch::text(etch::str_lit("Ctrl K"), {.color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false});
#endif
            }
            etch::set_tooltip(k_id, app.ts("titlebar.command_palette", "Command Palette (Ctrl+K)"));
            if (hov) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(k_id)) {
                app.cmd_palette_open = true;
                app.cmd_search[0] = '\0';
                app.cmd_selected_idx = 0;
                etch::request_focus(etch::id("cmd_palette_input"));
            }
        }

        // Contextual Split-Pane Workbench triggers with bespoke duotone active states
        if (icon_toggle_btn(etch::id("tb_changes"), app.icons.file_diff, app.icons.file_diff_duotone,
                            app.inspector_open && app.inspector_tab == InspectorTab::Changes,
                            pal, ts, app.t("titlebar.wb_changes", "Workbench Changes & Preview (Alt+W)"), mutated_files)) {
            if (app.inspector_open && app.inspector_tab == InspectorTab::Changes) app.inspector_open = false;
            else {
                app.inspector_open = true;
                app.inspector_tab = InspectorTab::Changes;
            }
        }
        if (icon_toggle_btn(etch::id("tb_agents"), app.icons.agents, app.icons.agents_duotone,
                            app.inspector_open && app.inspector_tab == InspectorTab::Agents,
                            pal, ts, app.t("titlebar.wb_agents", "Agent Hub (Alt+A)"), active_subagents)) {
            if (app.inspector_open && app.inspector_tab == InspectorTab::Agents) app.inspector_open = false;
            else {
                app.inspector_open = true;
                app.inspector_tab = InspectorTab::Agents;
                if (!app.showcase_mode) app.client.refresh_subagents();
            }
        }
        if (icon_toggle_btn(etch::id("tb_tree"), app.icons.git_graph, app.icons.git_graph_duotone,
                            app.inspector_open && app.inspector_tab == InspectorTab::Tree,
                            pal, ts, app.t("titlebar.wb_tree", "Session Branch Tree (Alt+T)"))) {
            if (app.inspector_open && app.inspector_tab == InspectorTab::Tree) app.inspector_open = false;
            else {
                app.inspector_open = true;
                app.inspector_tab = InspectorTab::Tree;
                if (!app.showcase_mode) app.client.refresh_tree();
            }
        }
        if (icon_toggle_btn(etch::id("tb_btw"), app.icons.btw, app.icons.btw_duotone,
                            app.inspector_open && app.inspector_tab == InspectorTab::Btw,
                            pal, ts, app.t("titlebar.wb_btw", "Side Question /btw (Alt+B)"))) {
            if (app.inspector_open && app.inspector_tab == InspectorTab::Btw) app.inspector_open = false;
            else {
                app.inspector_open = true;
                app.inspector_tab = InspectorTab::Btw;
            }
        }

#ifndef __APPLE__
        etch::box({
            .width = etch::Sizing::fixed(1.0f),
            .height = etch::Sizing::fixed(14.0f),
            .background = pal.border_subtle,
            .margin = {.left = 4.0f, .right = 2.0f},
        });

        bool c_min = false, c_max = false, c_close = false;
        const bool is_max = etch::is_window_maximized(app.ctx, app.window) ||
                            etch::is_window_fullscreen(app.ctx, app.window);
        win_chrome_btn(etch::id("win_min"), app.icons.win_min, false, pal,
                       app.t("window.minimize", "Minimize"), c_min);
        win_chrome_btn(etch::id("win_max"), is_max ? app.icons.win_restore : app.icons.win_max, false, pal,
                       is_max ? app.t("window.restore", "Restore") : app.t("window.maximize", "Maximize"), c_max);
        etch::window_maximize_button_region(etch::id("win_max"));
        win_chrome_btn(etch::id("win_close"), app.icons.win_close, true, pal,
                       app.t("window.close", "Close"), c_close);

        if (c_min && !app.headless_mode) {
            app.pending_win_action = App::PendingWindowAction::Minimize;
        }
        if (c_max && !app.headless_mode) {
            app.pending_win_action = App::PendingWindowAction::ToggleMaximize;
        }
        if (c_close && !app.headless_mode) {
            app.running = false;
        }
#endif
    }
}

} // namespace ompe
