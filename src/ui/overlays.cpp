#include "ui/app.h"
#include "ui/thinking_ui.h"
#include "core/platform.h"

#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace ompe {
namespace {

constexpr uint32_t kDialogOpenWorkspace = 101u;

bool icontains_sv(std::string_view hay, const char* needle) {
    if (!needle || needle[0] == '\0') return true;
    const size_t nlen = std::strlen(needle);
    if (nlen > hay.size()) return false;
    for (size_t i = 0; i + nlen <= hay.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < nlen; ++j) {
            if (std::tolower(static_cast<unsigned char>(hay[i + j])) !=
                std::tolower(static_cast<unsigned char>(needle[j]))) {
                ok = false;
                break;
            }
        }
        if (ok) return true;
    }
    return false;
}

void build_model_popover(App& app, const Palette& pal) {
    if (!app.model_popover_open) return;
    const TypeScale ts = app.type();
    const uint32_t pop_id = etch::id("model_popover");
    const uint32_t trig_id = etch::id("cmp_model_pill");

    ETCH_BOX({
        .id = pop_id,
        .axis = etch::Axis::Col,
        .width = etch::Sizing::fixed(356.0f),
        .height = etch::Sizing::fit(0.0f, 380.0f),
        .padding = etch::Padding::all(8.0f),
        .gap = 6.0f,
        .background = pal.bg_elevated,
        .border_color = pal.border_strong,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .shadow_color = pal.shadow,
        .shadow_blur = 28.0f,
        .shadow_offset = {0.0f, 10.0f},
        .attach_to = etch::AttachTo::Element,
        .attach_element_id = trig_id,
        .anchor_element = etch::Anchor::BottomLeft,
        .anchor_parent = etch::Anchor::TopLeft,
        .floating_offset = {0.0f, -8.0f},
        .z_index = 210,
        .clamp_to_viewport = true,
        .continuous_corners = true,
        .top_rim_alpha = pal.dark ? 0.16f : 0.0f,
    }) {
        etch::text_input({
            .id = etch::id("model_pop_search"),
            .buffer = app.model_search,
            .capacity = sizeof(app.model_search),
            .placeholder = app.ts("overlay.model.search", "Search models or providers..."),
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fixed(ts.row_h),
            .padding = etch::Padding::xy(8.0f, 4.0f),
            .font_size_px = ts.ui_sm,
            .background = pal.bg_input,
            .border_color = pal.border_subtle,
            .focus_border_color = pal.border_strong,
            .radius = etch::Corners::all(kRadiusSm),
            .focus_glow_blur = 0.0f,
            .leading_icon_id = app.icons.search,
            .leading_icon_color = pal.text_faint,
        });

        ETCH_BOX({
            .id = etch::id("model_pop_list"),
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fit(0.0f, 248.0f),
            .gap = 2.0f,
            .clip_children = true,
            .scroll_y = true,
            .scroll_fade_px = 14.0f,
        }) {
            const auto& models = app.client.available_models();
            if (models.empty()) {
                etch::text(app.ts("overlay.model.empty", "No models discovered yet. Add a provider below."), {
                    .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true
                });
            }
            for (size_t i = 0; i < models.size(); ++i) {
                const auto& m = models[i];
                if (!icontains_sv(m.name, app.model_search) &&
                    !icontains_sv(m.id, app.model_search) &&
                    !icontains_sv(m.provider, app.model_search)) {
                    continue;
                }
                const bool active = (m.id == app.client.state().model.id && m.provider == app.client.state().model.provider);
                const uint32_t mid = etch::id_index(etch::id("mdl_row"), static_cast<uint32_t>(i));
                const bool hov = etch::hovered(mid);

                ETCH_BOX({
                    .id = mid,
                    .axis = etch::Axis::Row,
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::fixed(ts.row_h + 6.0f),
                    .padding = etch::Padding::xy(9.0f, 0.0f),
                    .gap = 8.0f,
                    .align_y = etch::AlignY::Center,
                    .background = active ? pal.selected : (hov ? pal.hover : etch::Color::transparent()),
                    .radius = etch::Corners::all(kRadiusSm),
                    .continuous_corners = true,
                }) {
                    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .shrink_width = true}) {
                        etch::text(etch::str_cstr(m.name.c_str()), {
                            .color = active ? pal.accent : pal.text_primary,
                            .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false, .ellipsis = true
                        });
                        etch::text(etch::str_cstr(m.provider.c_str()), {
                            .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                        });
                    }
                    if (m.reasoning) {
                        etch::icon({.icon_id = app.icons.brain_circuit, .size_px = 12.0f, .color = pal.accent});
                    }
                    if (active) {
                        etch::icon({.icon_id = app.icons.badge_check, .size_px = 13.0f, .color = pal.accent});
                    }
                }
                if (hov) etch::set_cursor(etch::Cursor::Hand);
                if (etch::clicked(mid)) {
                    app.client.state().model = m;
                    if (!app.showcase_mode) app.client.set_model(m.provider, m.id);
                    app.model_popover_open = false;
                }
            }
        }

        // Direct product action: Add / Manage Provider
        const uint32_t add_p = etch::id("model_pop_add_prov");
        const bool ah = etch::hovered(add_p);
        ETCH_BOX({
            .id = add_p,
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fixed(30.0f),
            .padding = etch::Padding::xy(9.0f, 0.0f),
            .gap = 7.0f,
            .align_y = etch::AlignY::Center,
            .background = ah ? pal.hover : etch::Color::transparent(),
            .border_color = pal.border_subtle,
            .radius = etch::Corners::all(kRadiusSm),
            .border_sides = {.top = 1.0f},
        }) {
            etch::icon({.icon_id = app.icons.plus_circle, .size_px = 13.0f, .color = pal.accent});
            etch::text(app.ts("overlay.model.manage_providers", "Add or manage providers..."), {
                .color = pal.accent, .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = false
            });
        }
        if (ah) etch::set_cursor(etch::Cursor::Hand);
        if (etch::clicked(add_p)) {
            app.model_popover_open = false;
            app.settings_open = true;
            app.settings_tab = SettingsTab::Providers;
            app.provider_form.open = true;
        }
    }

    if (etch::should_close_popover(pop_id, trig_id)) {
        app.model_popover_open = false;
    }
}

void build_thinking_popover(App& app, const Palette& pal) {
    if (!app.thinking_popover_open) return;
    const TypeScale ts = app.type();
    const uint32_t pop_id = etch::id("thinking_popover");
    const uint32_t trig_id = etch::id("cmp_think_pill");

    const auto& lvls = app.client.available_thinking_levels();
    const bool supported = (lvls.size() > 1) || (!lvls.empty() && lvls[0] != "off");
    const std::string& cur = app.client.state().thinking_level;

    ETCH_BOX({
        .id = pop_id,
        .axis = etch::Axis::Col,
        .width = etch::Sizing::fixed(312.0f),
        .padding = etch::Padding::all(9.0f),
        .gap = 4.0f,
        .background = pal.bg_elevated,
        .border_color = pal.border_strong,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .shadow_color = pal.shadow,
        .shadow_blur = 28.0f,
        .shadow_offset = {0.0f, 10.0f},
        .attach_to = etch::AttachTo::Element,
        .attach_element_id = trig_id,
        .anchor_element = etch::Anchor::BottomLeft,
        .anchor_parent = etch::Anchor::TopLeft,
        .floating_offset = {0.0f, -8.0f},
        .z_index = 210,
        .clamp_to_viewport = true,
        .continuous_corners = true,
        .top_rim_alpha = pal.dark ? 0.16f : 0.0f,
    }) {
        // Header strip with shortcut keycap hint
        ETCH_BOX({
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .padding = {.left = 6.0f, .right = 4.0f, .top = 2.0f, .bottom = 5.0f},
            .align_y = etch::AlignY::Center,
        }) {
            etch::text(app.ts("overlay.thinking.heading", "THINKING EFFORT"), {
                .color = pal.text_faint,
                .font_size_px = ts.micro,
                .font_id = app.fonts.semibold,
                .letter_spacing = app.is_cjk_locale() ? 0.3f : 0.8f,
                .wrap = false,
            });
            etch::box({.width = etch::Sizing::grow()});
            ETCH_BOX({
                .padding = etch::Padding::xy(6.0f, 2.0f),
                .background = pal.bg_surface,
                .border_color = pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(4.0f),
            }) {
                etch::text(etch::str_lit("Ctrl+Shift+T"), {
                    .color = pal.text_faint,
                    .font_size_px = ts.micro,
                    .font_id = app.fonts.mono_tnum,
                    .wrap = false,
                });
            }
        }

        if (!supported) {
            ETCH_BOX({
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .padding = etch::Padding::xy(9.0f, 9.0f),
                .gap = 4.0f,
                .background = pal.bg_surface,
                .border_color = pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusSm),
            }) {
                etch::text(app.ts("overlay.thinking.unsupported_title", "Not supported by current model"), {
                    .color = pal.text_secondary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = true
                });
                etch::text(app.ts("overlay.thinking.unsupported_desc", "Switch to a reasoning-capable model to adjust thinking depth."), {
                    .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true
                });
            }
        } else {
            for (size_t i = 0; i < lvls.size(); ++i) {
                const std::string& lvl = lvls[i];
                const ThinkingLevelMeta meta = thinking_level_meta(lvl, app.i18n);
                const bool active = (cur == lvl) || (cur.empty() && lvl == "off");
                const uint32_t rid = etch::id_index(etch::id("thk_opt_row"), static_cast<uint32_t>(i));
                const bool hov = etch::hovered(rid);

                const etch::Color row_bg = active
                    ? (meta.cool_tint ? pal.secondary_soft : pal.selected)
                    : (hov ? pal.hover : etch::Color::transparent());
                const etch::Color row_border = active
                    ? (meta.bars == 0 ? pal.border_default
                                      : (meta.cool_tint ? etch::Color::rgba8(pal.secondary.r, pal.secondary.g, pal.secondary.b, 88)
                                                        : pal.accent_border))
                    : etch::Color::transparent();
                const etch::Color title_col = active
                    ? (meta.bars == 0 ? pal.text_primary : (meta.cool_tint ? pal.secondary : pal.accent))
                    : pal.text_primary;

                ETCH_BOX({
                    .id = rid,
                    .axis = etch::Axis::Row,
                    .width = etch::Sizing::grow(),
                    .padding = etch::Padding::xy(10.0f, 8.0f),
                    .gap = 10.0f,
                    .align_y = etch::AlignY::Center,
                    .background = row_bg,
                    .border_color = row_border,
                    .border_width = active ? 1.0f : 0.0f,
                    .radius = etch::Corners::all(kRadiusSm),
                    .continuous_corners = true,
                }) {
                    render_thinking_bars(meta, pal, false);
                    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 1.5f, .shrink_width = true}) {
                        etch::text(etch::str_cstr(meta.title), {
                            .color = title_col,
                            .font_size_px = ts.ui,
                            .font_id = active ? app.fonts.semibold : app.fonts.medium,
                            .wrap = false,
                        });
                        etch::text(etch::str_cstr(meta.description), {
                            .color = pal.text_muted,
                            .font_size_px = ts.meta,
                            .font_id = app.fonts.body,
                            .wrap = false,
                            .ellipsis = true,
                        });
                    }
                    if (active) {
                        etch::icon({
                            .icon_id = app.icons.badge_check,
                            .size_px = 14.5f,
                            .color = (meta.bars == 0) ? pal.text_secondary : (meta.cool_tint ? pal.secondary : pal.accent),
                        });
                    }
                }
                if (hov) etch::set_cursor(etch::Cursor::Hand);
                if (etch::clicked(rid)) {
                    const etch::Rect r = etch::element_bounds(rid);
                    app.set_thinking_effort(lvl, false, {r.x + 20.0f, r.y + r.h * 0.5f});
                    app.thinking_popover_open = false;
                }
            }
        }

        // Subtle contextual footer showing target model + Alt+Click hint
        const std::string mdl = app.client.state().model.name.empty()
            ? app.t("overlay.thinking.active_session", "Active session") : app.client.state().model.name;
        ETCH_BOX({
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .padding = {.left = 6.0f, .right = 6.0f, .top = 6.0f, .bottom = 2.0f},
            .align_y = etch::AlignY::Center,
            .border_color = pal.border_subtle,
            .border_sides = {.top = 1.0f},
        }) {
            ETCH_BOX({.width = etch::Sizing::grow(), .clip_children = true, .shrink_width = true}) {
                etch::text(etch::str_cstr(mdl.c_str()), {
                    .color = pal.text_faint,
                    .font_size_px = ts.meta,
                    .font_id = app.fonts.mono,
                    .wrap = false,
                    .ellipsis = true,
                });
            }
            etch::text(app.ts("overlay.thinking.alt_click_hint", "Alt+Click cycles"), {
                .color = pal.text_faint,
                .font_size_px = ts.micro,
                .font_id = app.fonts.body,
                .wrap = false,
            });
        }
    }

    if (etch::should_close_popover(pop_id, trig_id)) {
        app.thinking_popover_open = false;
    }
}

void build_workspace_popover(App& app, const Palette& pal) {
    if (!app.workspace_popover_open) return;
    const TypeScale ts = app.type();
    const uint32_t pop_id = etch::id("ws_popover");
    const uint32_t trig_id = etch::id("sb_workspace_btn");

    ETCH_BOX({
        .id = pop_id,
        .axis = etch::Axis::Col,
        .width = etch::Sizing::fixed(286.0f),
        .padding = etch::Padding::all(8.0f),
        .gap = 4.0f,
        .background = pal.bg_elevated,
        .border_color = pal.border_strong,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .shadow_color = pal.shadow,
        .shadow_blur = 24.0f,
        .shadow_offset = {0.0f, 8.0f},
        .attach_to = etch::AttachTo::Element,
        .attach_element_id = trig_id,
        .anchor_element = etch::Anchor::TopLeft,
        .anchor_parent = etch::Anchor::BottomLeft,
        .floating_offset = {0.0f, 6.0f},
        .z_index = 210,
        .clamp_to_viewport = true,
        .continuous_corners = true,
    }) {
        etch::text(app.ts("overlay.workspace.recent", "RECENT WORKSPACES"), {
            .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold,
            .letter_spacing = app.is_cjk_locale() ? 0.3f : 0.8f, .wrap = false
        });

        for (size_t i = 0; i < app.config.prefs().recent_workspaces.size(); ++i) {
            const std::string& w = app.config.prefs().recent_workspaces[i];
            const bool active = (w == app.config.prefs().active_workspace);
            const uint32_t wid = etch::id_index(etch::id("ws_item"), static_cast<uint32_t>(i));
            const bool hov = etch::hovered(wid);
            const std::string label = fs::path(w).filename().string();

            ETCH_BOX({
                .id = wid,
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .padding = etch::Padding::xy(9.0f, 6.0f),
                .gap = 1.0f,
                .background = active ? pal.selected : (hov ? pal.hover : etch::Color::transparent()),
                .radius = etch::Corners::all(kRadiusSm),
                .continuous_corners = true,
            }) {
                etch::text(etch::str_cstr(label.empty() ? w.c_str() : label.c_str()), {
                    .color = active ? pal.accent : pal.text_primary,
                    .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false, .ellipsis = true
                });
                etch::text(etch::str_cstr(w.c_str()), {
                    .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false, .ellipsis = true
                });
            }
            if (hov) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(wid)) {
                app.workspace_popover_open = false;
                if (!app.showcase_mode) app.start_omp_for_workspace(w);
                else app.config.prefs().active_workspace = w;
            }
        }

        const uint32_t open_id = etch::id("ws_open_folder");
        const bool oh = etch::hovered(open_id);
        ETCH_BOX({
            .id = open_id,
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fixed(ts.row_h),
            .padding = etch::Padding::xy(9.0f, 0.0f),
            .gap = 7.0f,
            .align_y = etch::AlignY::Center,
            .background = oh ? pal.hover : etch::Color::transparent(),
            .border_color = pal.border_subtle,
            .radius = etch::Corners::all(kRadiusSm),
            .border_sides = {.top = 1.0f},
        }) {
            etch::icon({.icon_id = app.icons.folder_open, .size_px = 13.0f, .color = pal.accent});
            etch::text(app.ts("overlay.workspace.open_folder", "Open Folder..."), {
                .color = pal.accent, .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = false
            });
        }
        if (oh) etch::set_cursor(etch::Cursor::Hand);
        if (etch::clicked(open_id) && !app.headless_mode) {
            etch::open_folder_dialog(app.ctx, app.window, app.config.prefs().active_workspace.c_str(), kDialogOpenWorkspace, false);
        }
    }

    if (etch::should_close_popover(pop_id, trig_id)) {
        app.workspace_popover_open = false;
    }
}

void build_command_palette(App& app, const Palette& pal) {
    if (!app.cmd_palette_open) return;
    const TypeScale ts = app.type();

    struct PaletteCmd {
        std::string title;
        std::string category;
        std::string aliases; // Locale-agnostic English keywords so searching EN or native always matches
        std::string shortcut;
        uint16_t icon;
        std::function<void()> run;
    };

    const char* cat_session    = app.t("cmd.cat.session",    "Session");
    const char* cat_model      = app.t("cmd.cat.model",      "Model");
    const char* cat_providers  = app.t("cmd.cat.providers",  "Providers");
    const char* cat_appearance = app.t("cmd.cat.appearance", "Appearance");
    const char* cat_typography = app.t("cmd.cat.typography", "Typography");
    const char* cat_language   = app.t("cmd.cat.language",   "Language");
    const char* cat_product    = app.t("cmd.cat.product",    "Product");
    const char* cat_workspace  = app.t("cmd.cat.workspace",  "Workspace");
    const char* cat_workbench  = app.t("cmd.cat.workbench",  "Workbench");

    std::vector<PaletteCmd> all_cmds = {
        {app.t("cmd.new_thread", "New Thread"), cat_session, "new thread session chat", "Ctrl+N", app.icons.square_pen, [&]() {
            if (!app.showcase_mode) app.client.new_session();
            else { app.client.messages().clear(); app.clear_doc_cache(); }
        }},
        {app.t("cmd.cycle_thinking", "Cycle Thinking Effort"), cat_model, "cycle thinking effort reasoning", "Ctrl+Shift+T", app.icons.gauge_effort, [&]() {
            app.cycle_thinking_effort();
        }},
        {app.t("cmd.choose_thinking", "Choose Thinking Effort..."), cat_model, "choose thinking effort depth reasoning", "", app.icons.brain_circuit, [&]() {
            app.thinking_popover_open = true;
            app.model_popover_open = false;
        }},
        {app.t("cmd.add_provider", "Add Custom Provider"), cat_providers, "add custom provider api key model", "", app.icons.plus_circle, [&]() {
            app.settings_open = true; app.settings_tab = SettingsTab::Providers; app.provider_form.open = true;
        }},
        {app.tf("cmd.switch_palette", "Switch Palette: {name}", {{"name", app.t("palette.obsidian-brass.name", "Obsidian Brass")}}),
         cat_appearance, "theme palette obsidian brass dark", "", app.icons.moon_dark, [&]() {
            app.config.prefs().palette = PaletteId::ObsidianBrass; app.apply_theme(); app.config.save_prefs();
        }},
        {app.tf("cmd.switch_palette", "Switch Palette: {name}", {{"name", app.t("palette.alabaster-ink.name", "Alabaster Ink")}}),
         cat_appearance, "theme palette alabaster ink light", "", app.icons.sun_light, [&]() {
            app.config.prefs().palette = PaletteId::AlabasterInk; app.apply_theme(); app.config.save_prefs();
        }},
        {app.tf("cmd.switch_palette", "Switch Palette: {name}", {{"name", app.t("palette.celadon-slate.name", "Celadon Slate")}}),
         cat_appearance, "theme palette celadon slate dark mineral", "", app.icons.gem_mineral, [&]() {
            app.config.prefs().palette = PaletteId::CeladonSlate; app.apply_theme(); app.config.save_prefs();
        }},
        {app.tf("cmd.switch_palette", "Switch Palette: {name}", {{"name", app.t("palette.nordic-linen.name", "Nordic Linen")}}),
         cat_appearance, "theme palette nordic linen light", "", app.icons.feather_linen, [&]() {
            app.config.prefs().palette = PaletteId::NordicLinen; app.apply_theme(); app.config.save_prefs();
        }},
        {app.t("cmd.zoom_in", "Increase Text Size"), cat_typography, "increase zoom font text size larger", "Ctrl+=", app.icons.zoom_in, [&]() {
            app.config.prefs().font_size_px = std::clamp(app.config.prefs().font_size_px + 1.0f, 13.0f, 20.0f);
            app.apply_theme(); app.config.save_prefs();
        }},
        {app.t("cmd.zoom_out", "Decrease Text Size"), cat_typography, "decrease zoom font text size smaller", "Ctrl+-", app.icons.zoom_out, [&]() {
            app.config.prefs().font_size_px = std::clamp(app.config.prefs().font_size_px - 1.0f, 13.0f, 20.0f);
            app.apply_theme(); app.config.save_prefs();
        }},
        {app.t("cmd.zoom_reset", "Reset Text Size (15px)"), cat_typography, "reset default font text size", "Ctrl+0", app.icons.type_reset, [&]() {
            app.config.prefs().font_size_px = 15.0f;
            app.apply_theme(); app.config.save_prefs();
        }},
    };

    // Dynamically register Language commands for Auto + EVERY discovered locale pack (en-US, zh-CN, or any drop-in locale!)
    all_cmds.push_back({
        app.tf("cmd.switch_language", "Switch Language: {name}", {{"name", app.t("settings.general.lang_auto", "Auto (System)")}}),
        cat_language,
        "language locale i18n auto system",
        "",
        app.icons.languages,
        [&]() { app.set_language_pref("auto"); }
    });
    for (const auto& loc : app.i18n.locales()) {
        all_cmds.push_back({
            app.tf("cmd.switch_language", "Switch Language: {name}", {{"name", loc.native_name + " (" + loc.code + ")"}}),
            cat_language,
            "language locale i18n " + loc.code + " " + loc.english_name + " " + loc.native_name,
            "",
            app.icons.languages,
            [&app, code = loc.code]() { app.set_language_pref(code); }
        });
    }

    all_cmds.push_back({app.t("cmd.open_settings", "Open Settings Center"), cat_product, "open settings preferences config", "Ctrl+,", app.icons.settings, [&]() {
        app.settings_open = true; app.sync_settings_buffers();
    }});
    all_cmds.push_back({app.t("cmd.open_workspace", "Open Folder / Workspace..."), cat_workspace, "open folder workspace directory", "", app.icons.folder_open, [&]() {
        if (!app.headless_mode) etch::open_folder_dialog(app.ctx, app.window, app.config.prefs().active_workspace.c_str(), kDialogOpenWorkspace, false);
    }});
    all_cmds.push_back({app.t("cmd.wb_changes", "Toggle Workbench Changes & Preview"), cat_workbench, "toggle workbench changes diff preview", "Alt+W", app.icons.file_diff, [&]() {
        app.inspector_open = true; app.inspector_tab = InspectorTab::Changes;
    }});
    all_cmds.push_back({app.t("cmd.wb_agents", "Toggle Agent Hub"), cat_workbench, "toggle subagent agent hub", "Alt+A", app.icons.agents, [&]() {
        app.inspector_open = true; app.inspector_tab = InspectorTab::Agents;
    }});
    all_cmds.push_back({app.t("cmd.wb_tree", "Toggle Session Branch Tree"), cat_workbench, "toggle session branch tree dag", "Alt+T", app.icons.git_graph, [&]() {
        app.inspector_open = true; app.inspector_tab = InspectorTab::Tree;
        if (!app.showcase_mode) app.client.refresh_tree();
    }});
    all_cmds.push_back({app.t("cmd.wb_btw", "Toggle /btw Side Question"), cat_workbench, "toggle btw side question", "Alt+B", app.icons.btw, [&]() {
        app.inspector_open = true; app.inspector_tab = InspectorTab::Btw;
    }});
    all_cmds.push_back({app.t("cmd.compact", "Compact Conversation Context"), cat_session, "compact summarize conversation context", "", app.icons.compress, [&]() {
        if (!app.showcase_mode) app.client.compact_now();
    }});
    all_cmds.push_back({app.t("cmd.about", "About OMP Desktop"), cat_product, "about omp desktop version diagnostics", "", app.icons.info, [&]() {
        app.about_open = true;
    }});

    for (const auto& sc : app.client.available_commands()) {
        all_cmds.push_back({
            "/" + sc.name + (sc.description.empty() ? "" : (" — " + sc.description)),
            app.t("cmd.cat.slash", "Slash"),
            sc.name + " " + sc.description,
            "",
            app.icons.command,
            [&app, name = sc.name]() {
                std::string cmd = "/" + name + " ";
                const size_t n = std::min(sizeof(app.composer_buf) - 1, cmd.size());
                std::memcpy(app.composer_buf, cmd.data(), n);
                app.composer_buf[n] = '\0';
                etch::request_focus(etch::id("composer_input"));
            }
        });
    }

    std::vector<const PaletteCmd*> filtered;
    for (const auto& c : all_cmds) {
        if (icontains_sv(c.title, app.cmd_search) ||
            icontains_sv(c.category, app.cmd_search) ||
            icontains_sv(c.aliases, app.cmd_search)) {
            filtered.push_back(&c);
        }
    }
    if (app.cmd_selected_idx >= static_cast<int>(filtered.size())) app.cmd_selected_idx = 0;
    if (etch::consume_key_pressed(SDLK_DOWN, true) && !filtered.empty()) {
        app.cmd_selected_idx = (app.cmd_selected_idx + 1) % static_cast<int>(filtered.size());
    }
    if (etch::consume_key_pressed(SDLK_UP, true) && !filtered.empty()) {
        app.cmd_selected_idx = (app.cmd_selected_idx - 1 + static_cast<int>(filtered.size())) % static_cast<int>(filtered.size());
    }

    // Scrim backdrop
    const uint32_t scrim_id = etch::id("cmd_scrim");
    ETCH_BOX({
        .id = scrim_id,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::grow(),
        .align_x = etch::AlignX::Center,
        .background = pal.scrim,
        .attach_to = etch::AttachTo::Root,
        .anchor_element = etch::Anchor::TopLeft,
        .anchor_parent = etch::Anchor::TopLeft,
        .z_index = 240,
        .backdrop_blur = 6.0f,
    }) {
        ETCH_BOX({
            .id = etch::id("cmd_modal"),
            .axis = etch::Axis::Col,
            .width = etch::Sizing::fixed(540.0f),
            .padding = etch::Padding::all(10.0f),
            .gap = 8.0f,
            .background = pal.bg_elevated,
            .border_color = pal.border_strong,
            .border_width = 1.0f,
            .radius = etch::Corners::all(kRadiusLg),
            .shadow_color = pal.shadow,
            .shadow_blur = 36.0f,
            .shadow_offset = {0.0f, 16.0f},
            .continuous_corners = true,
            .top_rim_alpha = pal.dark ? 0.20f : 0.0f,
            .margin = {.top = 84.0f},
        }) {
            auto ir = etch::text_input({
                .id = etch::id("cmd_palette_input"),
                .buffer = app.cmd_search,
                .capacity = sizeof(app.cmd_search),
                .placeholder = app.ts("overlay.cmd.placeholder", "Type a command, theme, language, or /slash..."),
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::fixed(38.0f),
                .padding = etch::Padding::xy(10.0f, 6.0f),
                .font_size_px = ts.body,
                .background = pal.bg_input,
                .border_color = pal.border_subtle,
                .focus_border_color = pal.border_strong,
                .radius = etch::Corners::all(kRadiusSm),
                .suppress_popup_keys = true,
                .focus_glow_blur = 0.0f,
                .leading_icon_id = app.icons.search,
                .leading_icon_color = pal.text_faint,
            });
            if (ir.submitted && !filtered.empty()) {
                auto fn = filtered[static_cast<size_t>(app.cmd_selected_idx)]->run;
                app.cmd_palette_open = false;
                fn();
                return;
            }

            ETCH_BOX({
                .id = etch::id("cmd_list_scroll"),
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::fit(0.0f, 340.0f),
                .gap = 2.0f,
                .clip_children = true,
                .scroll_y = true,
                .scroll_fade_px = 14.0f,
            }) {
                for (size_t i = 0; i < filtered.size() && i < 24; ++i) {
                    const auto& c = *filtered[i];
                    const uint32_t rid = etch::id_index(etch::id("cmd_row"), static_cast<uint32_t>(i));
                    const bool sel = (static_cast<int>(i) == app.cmd_selected_idx);
                    const bool hov = etch::hovered(rid);
                    if (hov) app.cmd_selected_idx = static_cast<int>(i);

                    ETCH_BOX({
                        .id = rid,
                        .axis = etch::Axis::Row,
                        .width = etch::Sizing::grow(),
                        .height = etch::Sizing::fixed(ts.row_h + 2.0f),
                        .padding = etch::Padding::xy(10.0f, 0.0f),
                        .gap = 10.0f,
                        .align_y = etch::AlignY::Center,
                        .background = sel ? pal.selected : etch::Color::transparent(),
                        .radius = etch::Corners::all(kRadiusSm),
                        .continuous_corners = true,
                    }) {
                        etch::icon({.icon_id = c.icon, .size_px = 13.5f, .color = sel ? pal.accent : pal.text_muted});
                        etch::text(etch::str_cstr(c.title.c_str()), {
                            .color = sel ? pal.text_primary : pal.text_secondary,
                            .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false, .ellipsis = true
                        });
                        etch::box({.width = etch::Sizing::grow()});
                        if (!c.shortcut.empty()) {
                            etch::text(etch::str_cstr(c.shortcut.c_str()), {
                                .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                            });
                        } else {
                            etch::text(etch::str_cstr(c.category.c_str()), {
                                .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                            });
                        }
                    }
                    if (hov) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(rid)) {
                        auto fn = c.run;
                        app.cmd_palette_open = false;
                        fn();
                        break;
                    }
                }
            }
        }
    }
    if (etch::clicked(scrim_id) && !etch::hovered(etch::id("cmd_modal"))) {
        app.cmd_palette_open = false;
    }
}

void build_toasts(App& app, const Palette& pal) {
    if (app.client.toasts().empty()) return;
    const TypeScale ts = app.type();

    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::fit(0.0f, 340.0f),
        .gap = 6.0f,
        .attach_to = etch::AttachTo::Root,
        .anchor_element = etch::Anchor::BottomRight,
        .anchor_parent = etch::Anchor::BottomRight,
        .floating_offset = {-20.0f, -20.0f},
        .z_index = 260,
        .pointer_passthrough = true,
    }) {
        uint64_t dismiss_id = 0;
        etch::Rect dismiss_rect{};
        etch::Color dismiss_col = pal.accent;

        for (const auto& t : app.client.toasts()) {
            const uint32_t tid = etch::id_index(etch::id("toast"), static_cast<uint32_t>(t.id));
            const etch::Color dot =
                (t.level == "error") ? pal.danger :
                (t.level == "warning") ? pal.warning :
                (t.level == "success") ? pal.success : pal.accent;

            ETCH_BOX({
                .id = tid,
                .axis = etch::Axis::Row,
                .padding = etch::Padding::xy(12.0f, 9.0f),
                .gap = 9.0f,
                .align_y = etch::AlignY::Center,
                .background = pal.bg_elevated,
                .border_color = pal.border_strong,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusMd),
                .shadow_color = pal.shadow,
                .shadow_blur = 20.0f,
                .shadow_offset = {0.0f, 8.0f},
                .continuous_corners = true,
            }) {
                etch::box({
                    .width = etch::Sizing::fixed(6.0f),
                    .height = etch::Sizing::fixed(6.0f),
                    .background = dot,
                    .radius = etch::Corners::all(3.0f),
                });
                etch::text(etch::str_cstr(t.text.c_str()), {
                    .color = pal.text_primary, .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = true
                });
            }
            if (etch::hovered(tid)) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(tid)) {
                dismiss_id = t.id;
                dismiss_rect = etch::element_bounds(tid);
                dismiss_col = dot;
            }
        }
        if (dismiss_id != 0) {
            app.particles.spawn_dissolve_rect(dismiss_rect, pal.bg_elevated, dismiss_col, DissolveStyle::Dust, 46);
            app.client.dismiss_toast(dismiss_id);
        }
    }
}

} // namespace

void build_overlays(App& app) {
    const Palette& pal = app.palette();
    const TypeScale ts = app.type();
    build_model_popover(app, pal);
    build_thinking_popover(app, pal);
    build_workspace_popover(app, pal);
    build_command_palette(app, pal);
    build_toasts(app, pal);

    // OAuth Browser Launch Confirmation Banner (if RPC emitted open_url)
    if (app.oauth_banner_open) {
        ETCH_BOX({
            .id = etch::id("oauth_banner"),
            .axis = etch::Axis::Col,
            .width = etch::Sizing::fixed(420.0f),
            .padding = etch::Padding::all(14.0f),
            .gap = 8.0f,
            .background = pal.bg_elevated,
            .border_color = pal.accent_border,
            .border_width = 1.0f,
            .radius = etch::Corners::all(kRadiusMd),
            .shadow_color = pal.shadow,
            .shadow_blur = 28.0f,
            .attach_to = etch::AttachTo::Root,
            .anchor_element = etch::Anchor::TopCenter,
            .anchor_parent = etch::Anchor::TopCenter,
            .floating_offset = {0.0f, 52.0f},
            .z_index = 255,
            .continuous_corners = true,
        }) {
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
                etch::icon({.icon_id = app.icons.external, .size_px = 14.0f, .color = pal.accent});
                etch::text(app.ts("overlay.oauth.title", "Browser Sign-In Opened"), {
                    .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.semibold, .wrap = false
                });
                etch::box({.width = etch::Sizing::grow()});
                if (etch::button({.id = etch::id("oauth_close"), .label = app.ts("common.close", "Close"),
                                  .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(24.0f)})) {
                    app.oauth_banner_open = false;
                }
            }
            if (!app.oauth_banner_req.instructions.empty()) {
                etch::text(etch::str_cstr(app.oauth_banner_req.instructions.c_str()), {
                    .color = pal.text_secondary, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true
                });
            }
            const std::string& target = !app.oauth_banner_req.launch_url.empty()
                ? app.oauth_banner_req.launch_url : app.oauth_banner_req.url;
            ETCH_BOX({.axis = etch::Axis::Row, .gap = 8.0f}) {
                if (etch::button({.id = etch::id("oauth_reopen"), .label = app.ts("overlay.oauth.open_browser", "Open Browser"),
                                  .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(28.0f)})) {
                    open_external_url(target);
                }
                if (etch::button({.id = etch::id("oauth_copy"), .label = app.ts("overlay.oauth.copy_link", "Copy Sign-In Link"),
                                  .variant = etch::ButtonVariant::Default, .height = etch::Sizing::fixed(28.0f)})) {
                    app.copy_with_feedback("oauth_url", target, etch::pointer_pos());
                }
            }
        }
    }
}

} // namespace ompe
