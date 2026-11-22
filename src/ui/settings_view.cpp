#include "ui/app.h"
#include "ui/thinking_ui.h"
#include "core/platform.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>

namespace ompe {
namespace {

constexpr uint32_t kDialogOpenWorkspace = 101u;
constexpr uint32_t kDialogPickOmpExe    = 103u;

void fill_buf(char* dst, size_t cap, const std::string& val) {
    if (!dst || cap == 0) return;
    const size_t n = std::min(cap - 1, val.size());
    std::memcpy(dst, val.data(), n);
    dst[n] = '\0';
}

void section_heading(const char* title, const char* subtitle, const Palette& pal, const FontSlots& fonts, const TypeScale& ts) {
    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 4.0f, .margin = {.bottom = 6.0f}}) {
        etch::text(etch::str_cstr(title), {
            .color = pal.text_primary, .font_size_px = ts.h1 - 2.0f, .font_id = fonts.semibold, .wrap = false
        });
        if (subtitle && subtitle[0] != '\0') {
            etch::text(etch::str_cstr(subtitle), {
                .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = fonts.body, .wrap = true
            });
        }
    }
}

void build_general_tab(App& app, const Palette& pal) {
    const TypeScale ts = app.type();
    const float ls = app.is_cjk_locale() ? 0.3f : 0.8f;
    section_heading(app.t("settings.general.title", "General"),
                    app.t("settings.general.subtitle", "Language, reading comfort, everyday workspace, and session defaults."),
                    pal, app.fonts, ts);

    // 0. Extensible Interface Language & Locale Pack Selector (Auto + all discovered BCP-47 JSON bundles)
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(16.0f),
        .gap = 10.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
            etch::icon({.icon_id = app.icons.languages, .size_px = 14.0f, .color = pal.accent});
            etch::text(app.ts("settings.general.lang_heading", "LANGUAGE & LOCALE"), {
                .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false
            });
            etch::box({.width = etch::Sizing::grow()});
            if (etch::button({.id = etch::id("gen_lang_reload"), .label = app.ts("common.reload", "Reload"),
                              .icon_id = app.icons.refresh, .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(22.0f)})) {
                app.reload_locales();
                app.client.push_toast(app.tf("toast.locales_reloaded", "Loaded {n} locale packs",
                                             {{"n", std::to_string(app.i18n.locales().size())}}), "info", 2000);
            }
            if (!app.i18n.user_locales_dir().empty() &&
                etch::button({.id = etch::id("gen_lang_dir"), .label = app.ts("settings.general.locales_dir", "Locales Folder"),
                              .icon_id = app.icons.external, .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(22.0f)})) {
                std::error_code ec;
                std::filesystem::create_directories(app.i18n.user_locales_dir(), ec);
                reveal_in_file_manager(app.i18n.user_locales_dir());
            }
        }

        // Dynamic pill selector: Auto (System) + every discovered locale bundle in assets/locales & ~/.omp/desktop/locales
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 7.0f, .wrap = true}) {
            const bool is_auto = (app.config.prefs().language.empty() || app.config.prefs().language == "auto");
            {
                const uint32_t aid = etch::id("gen_lang_auto");
                const bool hov = etch::hovered(aid);
                const std::string auto_lbl = app.tf("settings.general.lang_auto_sys", "Auto ({sys})",
                                                    {{"sys", app.i18n.system_locale()}});
                ETCH_BOX({
                    .id = aid,
                    .axis = etch::Axis::Row,
                    .height = etch::Sizing::fixed(ts.row_h - 2.0f),
                    .padding = etch::Padding::xy(11.0f, 0.0f),
                    .gap = 6.0f,
                    .align_y = etch::AlignY::Center,
                    .background = is_auto ? pal.selected : (hov ? pal.hover : pal.bg_canvas),
                    .border_color = is_auto ? pal.accent_border : pal.border_subtle,
                    .border_width = 1.0f,
                    .radius = etch::Corners::all(kRadiusSm),
                    .continuous_corners = true,
                }) {
                    if (is_auto) etch::icon({.icon_id = app.icons.badge_check, .size_px = 12.5f, .color = pal.accent});
                    etch::text(etch::str_cstr(auto_lbl.c_str()), {
                        .color = is_auto ? pal.accent : pal.text_secondary,
                        .font_size_px = ts.ui_sm,
                        .font_id = is_auto ? app.fonts.semibold : app.fonts.medium,
                        .wrap = false,
                    });
                }
                if (hov) etch::set_cursor(etch::Cursor::Hand);
                if (etch::clicked(aid)) app.set_language_pref("auto");
            }

            const auto& locs = app.i18n.locales();
            for (size_t i = 0; i < locs.size(); ++i) {
                const auto& loc = locs[i];
                const bool active = (!is_auto && app.i18n.active_code() == loc.code);
                const uint32_t lid = etch::id_index(etch::id("gen_lang_opt"), static_cast<uint32_t>(i));
                const bool hov = etch::hovered(lid);
                ETCH_BOX({
                    .id = lid,
                    .axis = etch::Axis::Row,
                    .height = etch::Sizing::fixed(ts.row_h - 2.0f),
                    .padding = etch::Padding::xy(11.0f, 0.0f),
                    .gap = 6.0f,
                    .align_y = etch::AlignY::Center,
                    .background = active ? pal.selected : (hov ? pal.hover : pal.bg_canvas),
                    .border_color = active ? pal.accent_border : pal.border_subtle,
                    .border_width = 1.0f,
                    .radius = etch::Corners::all(kRadiusSm),
                    .continuous_corners = true,
                }) {
                    if (active) etch::icon({.icon_id = app.icons.badge_check, .size_px = 12.5f, .color = pal.accent});
                    etch::text(etch::str_cstr(loc.native_name.c_str()), {
                        .color = active ? pal.accent : pal.text_secondary,
                        .font_size_px = ts.ui_sm,
                        .font_id = active ? app.fonts.semibold : app.fonts.medium,
                        .wrap = false,
                    });
                    etch::text(etch::str_cstr(loc.code.c_str()), {
                        .color = pal.text_faint,
                        .font_size_px = ts.micro,
                        .font_id = app.fonts.mono,
                        .wrap = false,
                    });
                }
                if (hov) etch::set_cursor(etch::Cursor::Hand);
                if (etch::clicked(lid)) app.set_language_pref(loc.code);
            }
        }

        etch::text(app.ts("settings.general.lang_hint",
                          "Drop any BCP-47 JSON file (e.g. ja-JP.json, fr-FR.json) into ~/.omp/desktop/locales/ to add or override languages."), {
            .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.body, .wrap = true
        });
    }

    // 1. Typography & Reading Comfort (scales ENTIRE interface + reading flow; Ctrl +/- / Ctrl 0 also zoom!)
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(16.0f),
        .gap = 12.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .align_y = etch::AlignY::Center}) {
            etch::text(app.ts("settings.general.scale_heading", "READING & INTERFACE SCALE"), {
                .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false
            });
            etch::box({.width = etch::Sizing::grow()});
            etch::text(etch::str_lit("Ctrl + / - / 0"), {
                .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.mono_tnum, .wrap = false
            });
        }

        if (etch::slider({
                .id = etch::id("gen_font_size"),
                .label = app.ts("settings.general.text_size", "Text size (UI & conversation)"),
                .value = &app.config.prefs().font_size_px,
                .min_value = TypeScale::kMinBodyPx,
                .max_value = TypeScale::kMaxBodyPx,
                .step = 0.5f,
                .format = "%.1f px",
                .fill_color = pal.accent,
                .style = etch::SliderStyle::Track,
                .track_color = pal.border_default,
                .thumb_color = pal.dark ? pal.text_primary : etch::Color::white(),
                .track_height_px = 4.0f,
                .thumb_size_px = 14.0f,
                .show_value = true,
            })) {
            app.apply_theme();
            app.config.save_prefs();
        }
        if (!app.fonts.family_label.empty()) {
            const std::string tf_lbl = app.tf("settings.general.typeface", "Typeface: {family}", {{"family", app.fonts.family_label}});
            etch::text(etch::str_cstr(tf_lbl.c_str()), {
                .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
            });
        }
    }

    // 2. Session Behavior
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(16.0f),
        .gap = 12.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        etch::text(app.ts("settings.general.session_heading", "SESSION BEHAVIOR"), {
            .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false
        });

        // Clean right-aligned toggle rows (zero redundant nested wrappers)
        auto toggle_row = [&](const char* title, const char* desc, uint32_t id, bool& val) -> bool {
            bool changed = false;
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 12.0f, .align_y = etch::AlignY::Center}) {
                ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 2.0f}) {
                    etch::text(etch::str_cstr(title), {.color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false});
                    etch::text(etch::str_cstr(desc), {.color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true});
                }
                changed = etch::toggle_switch({
                    .id = id,
                    .value = &val,
                    .on_color = pal.accent,
                    .off_color = pal.bg_input,
                    .border_color = pal.border_default,
                });
            }
            return changed;
        };

        bool auto_cmp = app.client.state().auto_compaction;
        if (toggle_row(app.t("settings.general.auto_compact_title", "Automatic context compaction"),
                       app.t("settings.general.auto_compact_desc", "Summarize earlier turns automatically before approaching context window limits."),
                       etch::id("gen_auto_cmp"), auto_cmp)) {
            app.client.state().auto_compaction = auto_cmp;
            if (!app.showcase_mode) app.client.set_auto_compaction(auto_cmp);
        }

        etch::box({.width = etch::Sizing::grow(), .height = etch::Sizing::fixed(1.0f), .background = pal.border_subtle});

        bool fast = app.client.state().fast_mode_enabled;
        if (toggle_row(app.t("settings.general.fast_mode_title", "Fast inference lane"),
                       app.t("settings.general.fast_mode_desc", "Prefer low-latency priority endpoints when supported by the active model."),
                       etch::id("gen_fast_mode"), fast)) {
            app.client.state().fast_mode_enabled = fast;
            if (!app.showcase_mode) app.client.set_fast_mode(fast);
        }
    }

    // 3. Active Workspace
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(14.0f),
        .gap = 8.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        etch::text(app.ts("settings.general.workspace_heading", "ACTIVE WORKSPACE"), {
            .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false
        });
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 10.0f, .align_y = etch::AlignY::Center}) {
            etch::icon({.icon_id = app.icons.folder_open, .size_px = 14.0f, .color = pal.accent});
            ETCH_BOX({.width = etch::Sizing::grow(), .clip_children = true, .shrink_width = true}) {
                etch::text(etch::str_cstr(app.config.prefs().active_workspace.c_str()), {
                    .color = pal.text_secondary, .font_size_px = ts.code, .font_id = app.fonts.mono, .wrap = false, .ellipsis = true
                });
            }
            if (etch::button({.id = etch::id("gen_pick_ws"), .label = app.ts("settings.general.switch_folder", "Switch Folder..."),
                              .variant = etch::ButtonVariant::Default, .height = etch::Sizing::fixed(28.0f)}) && !app.headless_mode) {
                etch::open_folder_dialog(app.ctx, app.window, app.config.prefs().active_workspace.c_str(), kDialogOpenWorkspace, false);
            }
        }
    }

    // 4. Software Updates (restrained, opt-out startup check + manual trigger)
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(14.0f),
        .gap = 10.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        const auto upd = app.updater.snapshot();
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
            etch::text(app.ts("settings.general.updates_heading", "SOFTWARE UPDATES"), {
                .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false
            });
            etch::box({.width = etch::Sizing::grow()});
            const std::string ver_tag = std::string("v") + OMPE_VERSION_STRING;
            etch::text(etch::str_cstr(ver_tag.c_str()), {
                .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.mono_tnum, .wrap = false
            });
        }

        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 12.0f, .align_y = etch::AlignY::Center}) {
            ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 2.0f}) {
                etch::text(app.ts("settings.general.auto_update_title", "Check for updates on startup"),
                           {.color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false});
                etch::text(app.ts("settings.general.auto_update_desc", "Quietly check GitHub Releases without interrupting your workflow."),
                           {.color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true});
            }
            if (etch::toggle_switch({
                    .id = etch::id("gen_auto_update"),
                    .value = &app.config.prefs().check_updates_on_startup,
                    .on_color = pal.accent,
                    .off_color = pal.bg_input,
                    .border_color = pal.border_default,
                })) {
                app.config.save_prefs();
            }
        }

        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
            std::string status_line;
            etch::Color st_col = pal.text_muted;
            if (upd.state == UpdateState::Checking) {
                status_line = app.t("update.checking", "Checking GitHub Releases...");
                st_col = pal.accent;
            } else if (upd.state == UpdateState::UpToDate) {
                status_line = app.tf("update.up_to_date", "Up to date (v{ver})", {{"ver", OMPE_VERSION_STRING}});
                st_col = pal.success;
            } else if (upd.state == UpdateState::Available) {
                status_line = app.tf("update.available", "Version v{ver} is available", {{"ver", upd.latest_version}});
                st_col = pal.accent;
            } else if (upd.state == UpdateState::Downloading) {
                char pct[16];
                std::snprintf(pct, sizeof(pct), "%d", static_cast<int>(upd.download_progress * 100.0f));
                status_line = app.tf("update.downloading", "Downloading installer ({pct}%)...", {{"pct", pct}});
                st_col = pal.accent;
            } else if (upd.state == UpdateState::ReadyToInstall) {
                status_line = upd.sha256_verified
                    ? app.t("update.verified_sha256", "Installer ready · SHA-256 verified")
                    : app.t("update.ready", "Installer ready");
                st_col = pal.success;
            } else if (upd.state == UpdateState::Error) {
                status_line = upd.error_message;
                st_col = pal.text_muted;
            }
            if (!status_line.empty()) {
                etch::text(etch::str_cstr(status_line.c_str()), {
                    .color = st_col, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true
                });
            }
            etch::box({.width = etch::Sizing::grow()});
            if (upd.state == UpdateState::Available) {
                if (etch::button({.id = etch::id("gen_upd_dl"), .label = app.ts("update.download", "Download Installer"),
                                  .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(26.0f)})) {
                    app.updater.download_installer();
                }
            } else if (upd.state == UpdateState::ReadyToInstall) {
                if (etch::button({.id = etch::id("gen_upd_inst"), .label = app.ts("update.install_now", "Install Now"),
                                  .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(26.0f)})) {
                    std::string err;
                    if (!app.updater.launch_installer(&err)) {
                        app.client.push_toast(err, "error", 3200);
                    }
                }
            } else {
                const bool busy = (upd.state == UpdateState::Checking || upd.state == UpdateState::Downloading);
                if (etch::button({.id = etch::id("gen_upd_chk"), .label = app.ts("update.check", "Check for Updates"),
                                  .icon_id = app.icons.refresh, .variant = etch::ButtonVariant::Default,
                                  .height = etch::Sizing::fixed(26.0f), .disabled = busy})) {
                    app.updater.check_for_updates(OMPE_VERSION_STRING, OMPE_GITHUB_REPO);
                }
            }
        }
    }
}

void build_providers_tab(App& app, const Palette& pal) {
    const TypeScale ts = app.type();
    const float ls = app.is_cjk_locale() ? 0.3f : 0.8f;
    section_heading(app.t("settings.providers.title", "Providers & Accounts"),
                    app.t("settings.providers.subtitle", "Sign in via OAuth or add custom OpenAI / Anthropic / local gateways into models.yml."),
                    pal, app.fonts, ts);

    // 1. OAuth Accounts (compact, searchable, connected first — handles 97 real OMP providers gracefully)
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(12.0f),
        .gap = 8.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        const auto& lps = app.client.login_providers();
        size_t connected_count = 0;
        for (const auto& lp : lps) {
            if (lp.authenticated) ++connected_count;
        }

        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
            etch::text(app.ts("settings.providers.oauth_heading", "OAUTH PROVIDERS"), {
                .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false
            });
            if (!lps.empty()) {
                const std::string badge = app.tf("settings.providers.oauth_stats", "{connected} connected · {total} total", {
                    {"connected", std::to_string(connected_count)},
                    {"total", std::to_string(lps.size())},
                });
                etch::text(etch::str_cstr(badge.c_str()), {
                    .color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                });
            }
            etch::box({.width = etch::Sizing::grow()});
            if (lps.size() > 6) {
                etch::text_input({
                    .id = etch::id("prov_oauth_search"),
                    .buffer = app.oauth_search,
                    .capacity = sizeof(app.oauth_search),
                    .placeholder = app.ts("settings.providers.filter_placeholder", "Filter providers..."),
                    .width = etch::Sizing::fixed(172.0f),
                    .height = etch::Sizing::fixed(ts.pill_h),
                    .font_size_px = ts.ui_sm,
                    .focus_glow_blur = 0.0f,
                    .leading_icon_id = app.icons.search,
                    .leading_icon_color = pal.text_faint,
                });
            }
            if (etch::button({.id = etch::id("prov_refresh_oauth"), .label = app.ts("common.refresh", "Refresh"),
                              .icon_id = app.icons.refresh,
                              .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(24.0f)})) {
                if (!app.showcase_mode) app.client.refresh_login_providers();
            }
        }

        if (lps.empty()) {
            etch::text(app.ts("settings.providers.oauth_empty", "Connect to OMP engine to inspect OAuth providers."), {
                .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true
            });
        }

        std::string filter_low = app.oauth_search;
        std::transform(filter_low.begin(), filter_low.end(), filter_low.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        auto provider_priority = [](const LoginProviderInfo& p) -> int {
            if (p.authenticated) return 0;
            if (p.id == "anthropic" || p.id == "openai-codex" || p.id == "github-copilot" ||
                p.id == "google-gemini-cli" || p.id == "cursor" || p.id == "openrouter") {
                return 1;
            }
            return 2;
        };

        std::vector<size_t> order;
        order.reserve(lps.size());
        for (size_t i = 0; i < lps.size(); ++i) {
            if (!filter_low.empty()) {
                std::string hay = lps[i].name + " " + lps[i].id;
                std::transform(hay.begin(), hay.end(), hay.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (hay.find(filter_low) == std::string::npos) continue;
            }
            order.push_back(i);
        }
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            const int pa = provider_priority(lps[a]);
            const int pb = provider_priority(lps[b]);
            if (pa != pb) return pa < pb;
            return lps[a].name < lps[b].name;
        });

        const bool compact_cap = filter_low.empty() && !app.oauth_show_all;
        const size_t max_visible = compact_cap ? std::max<size_t>(connected_count + 4, 6) : order.size();
        const size_t show_count = std::min(order.size(), max_visible);

        for (size_t oi = 0; oi < show_count; ++oi) {
            const size_t i = order[oi];
            const auto& lp = lps[i];
            ETCH_BOX({
                .axis = etch::Axis::Row,
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::fixed(36.0f),
                .padding = etch::Padding::xy(10.0f, 0.0f),
                .gap = 10.0f,
                .align_y = etch::AlignY::Center,
                .background = pal.bg_canvas,
                .border_color = pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusSm),
                .continuous_corners = true,
            }) {
                etch::icon({
                    .icon_id = lp.authenticated ? app.icons.shield_check : app.icons.plug,
                    .size_px = 13.5f,
                    .color = lp.authenticated ? pal.success : pal.text_faint,
                });
                etch::text(etch::str_cstr(lp.name.c_str()), {
                    .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false
                });
                etch::text(lp.authenticated ? app.ts("settings.providers.connected", "Connected") : app.ts("settings.providers.not_signed_in", "Not signed in"), {
                    .color = lp.authenticated ? pal.success : pal.text_faint,
                    .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                });
                etch::box({.width = etch::Sizing::grow()});

                const uint32_t btn_id = etch::id_index(etch::id("oauth_signin"), static_cast<uint32_t>(i));
                if (etch::button({.id = btn_id,
                                  .label = lp.authenticated ? app.ts("settings.providers.add_account", "Add Account") : app.ts("settings.providers.sign_in", "Sign In"),
                                  .variant = lp.authenticated ? etch::ButtonVariant::Default : etch::ButtonVariant::Primary,
                                  .height = etch::Sizing::fixed(26.0f)})) {
                    if (!app.showcase_mode) app.client.start_oauth_login(lp.id);
                    else app.client.push_toast(app.tf("toast.oauth_demo", "OAuth browser flow triggered for {name}", {{"name", lp.name}}), "info", 2600);
                }
                if (lp.authenticated) {
                    const uint32_t m_id = etch::id_index(etch::id("oauth_manage"), static_cast<uint32_t>(i));
                    if (etch::button({.id = m_id, .label = app.ts("settings.providers.accounts", "Accounts"),
                                      .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(26.0f)})) {
                        app.manage_accounts_provider = (app.manage_accounts_provider == lp.id) ? "" : lp.id;
                        if (!app.manage_accounts_provider.empty() && !app.showcase_mode) {
                            app.client.query_logout_accounts(lp.id);
                        }
                    }
                }
            }

            if (app.manage_accounts_provider == lp.id && !app.client.logout_accounts().empty()) {
                ETCH_BOX({
                    .axis = etch::Axis::Col,
                    .width = etch::Sizing::grow(),
                    .padding = {.left = 18.0f, .right = 8.0f, .top = 2.0f, .bottom = 4.0f},
                    .gap = 4.0f,
                }) {
                    for (size_t ai = 0; ai < app.client.logout_accounts().size(); ++ai) {
                        const auto& acc = app.client.logout_accounts()[ai];
                        if (acc.provider != lp.id) continue;
                        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
                            etch::text(etch::str_cstr((acc.label + " (" + acc.type + ")").c_str()), {
                                .color = pal.text_secondary, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                            });
                            etch::box({.width = etch::Sizing::grow()});
                            const uint32_t lo_id = etch::id_index(etch::id("oauth_lo"), static_cast<uint32_t>(ai));
                            if (etch::button({.id = lo_id, .label = app.ts("settings.providers.sign_out", "Sign Out"),
                                              .variant = etch::ButtonVariant::Danger, .height = etch::Sizing::fixed(22.0f)})) {
                                if (!app.showcase_mode) app.client.logout_account(acc.provider, acc.credential_id);
                            }
                        }
                    }
                }
            }
        }

        if (filter_low.empty() && order.size() > std::max<size_t>(connected_count + 4, 6)) {
            const std::string toggle_lbl = app.oauth_show_all
                ? std::string(app.t("settings.providers.show_fewer", "Show fewer providers"))
                : app.tf("settings.providers.show_all", "Show all {n} providers", {{"n", std::to_string(order.size())}});
            if (etch::button({.id = etch::id("oauth_toggle_all"),
                              .label = etch::str_cstr(toggle_lbl.c_str()),
                              .variant = etch::ButtonVariant::Ghost,
                              .height = etch::Sizing::fixed(26.0f)})) {
                app.oauth_show_all = !app.oauth_show_all;
            }
        } else if (!filter_low.empty() && order.empty()) {
            etch::text(app.ts("settings.providers.no_match", "No OAuth providers match that filter."), {
                .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = false
            });
        }
    }

    // 2. Custom & Local Providers (`models.yml`)
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(12.0f),
        .gap = 10.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .align_y = etch::AlignY::Center}) {
            ETCH_BOX({.axis = etch::Axis::Col, .gap = 2.0f}) {
                etch::text(app.ts("settings.providers.custom_heading", "CUSTOM & LOCAL PROVIDERS"), {
                    .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false
                });
                etch::text(etch::str_cstr(app.config.models_yml_path().c_str()), {
                    .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                });
            }
            etch::box({.width = etch::Sizing::grow()});
            if (etch::button({.id = etch::id("prov_add_btn"),
                              .label = app.provider_form.open ? app.ts("settings.providers.close_form", "Close Form") : app.ts("settings.providers.add_provider", "Add Provider"),
                              .icon_id = app.provider_form.open ? app.icons.close : app.icons.plus_circle,
                              .variant = etch::ButtonVariant::Primary,
                              .height = etch::Sizing::fixed(28.0f)})) {
                app.provider_form.open = !app.provider_form.open;
                if (app.provider_form.open) {
                    app.provider_form.editing = false;
                    app.provider_form.error_msg.clear();
                }
            }
        }

        if (app.provider_form.open) {
            auto& pf = app.provider_form;
            ETCH_BOX({
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .padding = etch::Padding::all(12.0f),
                .gap = 9.0f,
                .background = pal.bg_canvas,
                .border_color = pal.border_default,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusMd),
                .continuous_corners = true,
            }) {
                struct Preset {
                    const char* label;
                    const char* id;
                    const char* url;
                    const char* key;
                    const char* models;
                    int api_idx;
                    int auth_idx;
                    int disc_idx;
                    bool reasoning;
                };
                const Preset presets[6] = {
                    {"OpenAI Compat", "custom-openai", "https://api.example.com/v1", "OPENAI_API_KEY", "gpt-4.1, gpt-4.1-mini", 0, 0, 1, false},
                    {"Anthropic Compat", "custom-claude", "https://api.anthropic.com", "ANTHROPIC_API_KEY", "claude-sonnet-4-5", 2, 0, 0, true},
                    {"OpenRouter", "openrouter", "https://openrouter.ai/api/v1", "OPENROUTER_API_KEY", "anthropic/claude-sonnet-4-5", 0, 0, 1, true},
                    {"DeepSeek", "deepseek", "https://api.deepseek.com/v1", "DEEPSEEK_API_KEY", "deepseek-chat, deepseek-reasoner", 0, 0, 0, true},
                    {"Ollama Local", "ollama-local", "http://127.0.0.1:11434/v1", "", "qwen2.5-coder:32b", 0, 1, 2, false},
                    {"LM Studio", "lm-studio", "http://127.0.0.1:1234/v1", "", "", 0, 1, 3, false},
                };

                ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 5.0f, .wrap = true}) {
                    for (uint32_t pi = 0; pi < 6; ++pi) {
                        const uint32_t pid = etch::id_index(etch::id("prov_preset"), pi);
                        const bool hov = etch::hovered(pid);
                        ETCH_BOX({
                            .id = pid,
                            .padding = etch::Padding::xy(8.0f, 4.0f),
                            .background = hov ? pal.hover : pal.bg_surface,
                            .border_color = hov ? pal.border_strong : pal.border_subtle,
                            .border_width = 1.0f,
                            .radius = etch::Corners::all(kRadiusPill),
                        }) {
                            etch::text(etch::str_cstr(presets[pi].label), {
                                .color = hov ? pal.text_primary : pal.text_secondary,
                                .font_size_px = ts.meta, .font_id = app.fonts.medium, .wrap = false
                            });
                        }
                        if (hov) etch::set_cursor(etch::Cursor::Hand);
                        if (etch::clicked(pid)) {
                            fill_buf(pf.id, sizeof(pf.id), presets[pi].id);
                            fill_buf(pf.base_url, sizeof(pf.base_url), presets[pi].url);
                            fill_buf(pf.api_key, sizeof(pf.api_key), presets[pi].key);
                            fill_buf(pf.models_csv, sizeof(pf.models_csv), presets[pi].models);
                            pf.api_index = presets[pi].api_idx;
                            pf.auth_index = presets[pi].auth_idx;
                            pf.discovery_index = presets[pi].disc_idx;
                            pf.reasoning = presets[pi].reasoning;
                        }
                    }
                }

                ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f}) {
                    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::fixed(170.0f), .gap = 4.0f}) {
                        etch::text(app.ts("settings.providers.field_id", "Provider ID"), {.color = pal.text_muted, .font_size_px = ts.meta, .wrap = false});
                        etch::text_input({.id = etch::id("pf_id"), .buffer = pf.id, .capacity = sizeof(pf.id),
                                          .placeholder = etch::str_lit("e.g. deepseek"), .height = etch::Sizing::fixed(ts.row_h - 2.0f),
                                          .font_size_px = ts.ui_sm, .focus_glow_blur = 0.0f});
                    }
                    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 4.0f}) {
                        etch::text(app.ts("settings.providers.field_url", "Base URL"), {.color = pal.text_muted, .font_size_px = ts.meta, .wrap = false});
                        etch::text_input({.id = etch::id("pf_url"), .buffer = pf.base_url, .capacity = sizeof(pf.base_url),
                                          .placeholder = etch::str_lit("https://api.example.com/v1"), .height = etch::Sizing::fixed(ts.row_h - 2.0f),
                                          .font_size_px = ts.ui_sm, .focus_glow_blur = 0.0f});
                    }
                }

                const etch::Str api_items[] = {
                    etch::str_lit("openai-completions"),
                    etch::str_lit("openai-responses"),
                    etch::str_lit("anthropic-messages"),
                    etch::str_lit("google-generative-ai"),
                };
                const etch::Str auth_items[] = {etch::str_lit("apiKey"), etch::str_lit("none"), etch::str_lit("oauth")};
                const etch::Str disc_items[] = {
                    app.ts("settings.providers.disc_manual", "Manual models"),
                    etch::str_lit("openai-models-list"),
                    etch::str_lit("ollama"),
                    etch::str_lit("lm-studio"),
                    etch::str_lit("llama.cpp"),
                };
                ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f}) {
                    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 4.0f}) {
                        etch::text(app.ts("settings.providers.field_api", "Wire API"), {.color = pal.text_muted, .font_size_px = ts.meta, .wrap = false});
                        etch::combo_box({.id = etch::id("pf_api"), .selected_index = &pf.api_index, .items = api_items,
                                         .height_px = ts.row_h - 2.0f, .border_color = pal.border_default, .focus_border_color = pal.accent_border});
                    }
                    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::fixed(114.0f), .gap = 4.0f}) {
                        etch::text(app.ts("settings.providers.field_auth", "Auth"), {.color = pal.text_muted, .font_size_px = ts.meta, .wrap = false});
                        etch::combo_box({.id = etch::id("pf_auth"), .selected_index = &pf.auth_index, .items = auth_items,
                                         .height_px = ts.row_h - 2.0f, .border_color = pal.border_default, .focus_border_color = pal.accent_border});
                    }
                    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 4.0f}) {
                        etch::text(app.ts("settings.providers.field_discovery", "Discovery"), {.color = pal.text_muted, .font_size_px = ts.meta, .wrap = false});
                        etch::combo_box({.id = etch::id("pf_disc"), .selected_index = &pf.discovery_index, .items = disc_items,
                                         .height_px = ts.row_h - 2.0f, .border_color = pal.border_default, .focus_border_color = pal.accent_border});
                    }
                }

                ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f}) {
                    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 4.0f}) {
                        etch::text(app.ts("settings.providers.field_key", "API Key or Env Var (e.g. MY_KEY or sk-...)"), {.color = pal.text_muted, .font_size_px = ts.meta, .wrap = false});
                        etch::text_input({.id = etch::id("pf_key"), .buffer = pf.api_key, .capacity = sizeof(pf.api_key),
                                          .placeholder = etch::str_lit("sk-... or ENV_VAR_NAME"),
                                          .height = etch::Sizing::fixed(ts.row_h - 2.0f), .font_size_px = ts.ui_sm, .focus_glow_blur = 0.0f});
                    }
                    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 4.0f}) {
                        etch::text(app.ts("settings.providers.field_models", "Model IDs (comma-separated)"), {.color = pal.text_muted, .font_size_px = ts.meta, .wrap = false});
                        etch::text_input({.id = etch::id("pf_models"), .buffer = pf.models_csv, .capacity = sizeof(pf.models_csv),
                                          .placeholder = etch::str_lit("model-a, model-b"),
                                          .height = etch::Sizing::fixed(ts.row_h - 2.0f), .font_size_px = ts.ui_sm, .focus_glow_blur = 0.0f});
                    }
                }

                ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 12.0f, .align_y = etch::AlignY::Center}) {
                    etch::toggle_switch({.id = etch::id("pf_reasoning"), .label = app.ts("settings.providers.field_reasoning", "Supports reasoning / thinking"),
                                         .value = &pf.reasoning, .on_color = pal.accent, .off_color = pal.bg_input, .border_color = pal.border_default});
                    etch::box({.width = etch::Sizing::grow()});
                    if (!pf.error_msg.empty()) {
                        etch::text(etch::str_cstr(pf.error_msg.c_str()), {.color = pal.danger, .font_size_px = ts.meta, .wrap = false});
                    }
                    if (etch::button({.id = etch::id("pf_save"), .label = app.ts("settings.providers.save_reload", "Save & Reload OMP"),
                                      .icon_id = app.icons.check, .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(30.0f)})) {
                        CustomProviderConfig cfg;
                        cfg.id = pf.id;
                        cfg.base_url = pf.base_url;
                        cfg.api_key = pf.api_key;
                        const char* apis[] = {"openai-completions", "openai-responses", "anthropic-messages", "google-generative-ai"};
                        const char* auths[] = {"apiKey", "none", "oauth"};
                        const char* discs[] = {"", "openai-models-list", "ollama", "lm-studio", "llama.cpp"};
                        cfg.api = apis[std::clamp(pf.api_index, 0, 3)];
                        cfg.auth = auths[std::clamp(pf.auth_index, 0, 2)];
                        cfg.discovery_type = discs[std::clamp(pf.discovery_index, 0, 4)];

                        std::stringstream ms(pf.models_csv);
                        std::string item;
                        while (std::getline(ms, item, ',')) {
                            while (!item.empty() && item.front() == ' ') item.erase(item.begin());
                            while (!item.empty() && item.back() == ' ') item.pop_back();
                            if (!item.empty()) {
                                cfg.models.push_back({item, item, pf.reasoning, 128000, 16384});
                            }
                        }

                        std::string err;
                        if (app.config.upsert_custom_provider(cfg, &err)) {
                            pf.open = false;
                            pf.error_msg.clear();
                            app.particles.spawn_point_spark(etch::pointer_pos(), pal.accent, 24);
                            app.client.push_toast(app.tf("toast.saved_provider", "Saved provider '{id}' · Reloading OMP catalog", {{"id", cfg.id}}), "success", 3400);
                            if (!app.showcase_mode && !app.client.state().is_streaming) {
                                app.client.reconnect();
                            }
                        } else {
                            pf.error_msg = err;
                        }
                    }
                }
            }
        }

        const auto& provs = app.config.custom_providers();
        if (provs.empty() && !app.provider_form.open) {
            etch::text(app.ts("settings.providers.custom_empty",
                              "No custom providers in models.yml yet. Click 'Add Provider' for OpenRouter, DeepSeek, Ollama, or any gateway."), {
                .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true
            });
        }
        std::string delete_id;
        etch::Rect delete_rect{};
        for (size_t i = 0; i < provs.size(); ++i) {
            const auto& p = provs[i];
            const uint32_t row_id = etch::id_index(etch::id("cprov_row"), static_cast<uint32_t>(i));
            ETCH_BOX({
                .id = row_id,
                .axis = etch::Axis::Row,
                .width = etch::Sizing::grow(),
                .padding = etch::Padding::xy(10.0f, 8.0f),
                .gap = 10.0f,
                .align_y = etch::AlignY::Center,
                .background = pal.bg_canvas,
                .border_color = pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusSm),
                .continuous_corners = true,
            }) {
                ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 2.0f, .shrink_width = true}) {
                    ETCH_BOX({.axis = etch::Axis::Row, .gap = 8.0f, .align_y = etch::AlignY::Center}) {
                        etch::text(etch::str_cstr(p.id.c_str()), {
                            .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.semibold, .wrap = false
                        });
                        etch::text(etch::str_cstr(p.api.c_str()), {
                            .color = pal.accent, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                        });
                    }
                    etch::text(etch::str_cstr(p.base_url.c_str()), {
                        .color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false, .ellipsis = true
                    });
                }

                const uint32_t ed_id = etch::id_index(etch::id("cprov_edit"), static_cast<uint32_t>(i));
                if (etch::button({.id = ed_id, .label = app.ts("common.edit", "Edit"), .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(24.0f)})) {
                    auto& pf = app.provider_form;
                    pf.open = true;
                    pf.editing = true;
                    fill_buf(pf.id, sizeof(pf.id), p.id);
                    fill_buf(pf.base_url, sizeof(pf.base_url), p.base_url);
                    fill_buf(pf.api_key, sizeof(pf.api_key), p.api_key);
                    std::string csv;
                    for (size_t mi = 0; mi < p.models.size(); ++mi) {
                        if (mi > 0) csv += ", ";
                        csv += p.models[mi].id;
                    }
                    fill_buf(pf.models_csv, sizeof(pf.models_csv), csv);
                    pf.api_index = (p.api == "openai-responses") ? 1 : (p.api == "anthropic-messages" ? 2 : (p.api == "google-generative-ai" ? 3 : 0));
                    pf.auth_index = (p.auth == "none") ? 1 : (p.auth == "oauth" ? 2 : 0);
                }

                const uint32_t rm_id = etch::id_index(etch::id("cprov_del"), static_cast<uint32_t>(i));
                if (etch::button({.id = rm_id, .label = app.ts("common.remove", "Remove"), .variant = etch::ButtonVariant::Danger, .height = etch::Sizing::fixed(24.0f)})) {
                    delete_id = p.id;
                    delete_rect = etch::element_bounds(row_id);
                }
            }
        }
        if (!delete_id.empty()) {
            app.particles.spawn_dissolve_rect(delete_rect, pal.bg_surface, pal.danger, DissolveStyle::Ember, 54);
            app.config.delete_custom_provider(delete_id);
            app.client.push_toast(app.tf("toast.removed_provider", "Removed provider '{id}'", {{"id", delete_id}}), "info", 2800);
            if (!app.showcase_mode && !app.client.state().is_streaming) app.client.reconnect();
        }
    }
}

void build_models_tab(App& app, const Palette& pal) {
    const TypeScale ts = app.type();
    const float ls = app.is_cjk_locale() ? 0.3f : 0.8f;
    section_heading(app.t("settings.models.title", "Models & Roles"),
                    app.t("settings.models.subtitle", "Assign OMP model roles in config.yml and choose default reasoning effort."),
                    pal, app.fonts, ts);

    // 1. Live Discovered Model Quick-Assign Catalog Chips (click any discovered model to assign as Default!)
    const auto& catalog = app.client.available_models();
    if (!catalog.empty()) {
        ETCH_BOX({
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .padding = etch::Padding::all(12.0f),
            .gap = 8.0f,
            .background = pal.bg_surface,
            .border_color = pal.border_subtle,
            .border_width = 1.0f,
            .radius = etch::Corners::all(kRadiusMd),
            .continuous_corners = true,
        }) {
            etch::text(app.ts("settings.models.quick_pick", "QUICK-PICK FROM DISCOVERED CATALOG (SETS DEFAULT ROLE)"), {
                .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false
            });
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 6.0f, .wrap = true}) {
                const size_t lim = std::min<size_t>(catalog.size(), 10);
                for (size_t i = 0; i < lim; ++i) {
                    const auto& m = catalog[i];
                    const std::string sel_str = m.selector();
                    const bool is_def = (std::strcmp(app.role_default_buf, sel_str.c_str()) == 0);
                    const uint32_t cid = etch::id_index(etch::id("mdl_chip"), static_cast<uint32_t>(i));
                    const bool hov = etch::hovered(cid);
                    ETCH_BOX({
                        .id = cid,
                        .axis = etch::Axis::Row,
                        .padding = etch::Padding::xy(9.0f, 5.0f),
                        .gap = 5.0f,
                        .align_y = etch::AlignY::Center,
                        .background = is_def ? pal.active : (hov ? pal.hover : pal.bg_canvas),
                        .border_color = is_def ? pal.accent_border : pal.border_subtle,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(kRadiusPill),
                    }) {
                        if (m.reasoning) {
                            etch::icon({.icon_id = is_def ? app.icons.brain_active : app.icons.brain_circuit, .size_px = 11.5f, .color = pal.accent});
                        }
                        etch::text(etch::str_cstr(m.name.c_str()), {
                            .color = is_def ? pal.text_primary : pal.text_secondary,
                            .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = false
                        });
                    }
                    if (hov) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(cid)) {
                        fill_buf(app.role_default_buf, sizeof(app.role_default_buf), sel_str);
                        app.client.state().model = m;
                        if (!app.showcase_mode) {
                            app.client.set_model(m.provider, m.id);
                            std::string err;
                            app.config.set_omp_config_value(app.omp_path_buf, "modelRoles.default", sel_str, &err);
                        }
                        app.client.push_toast(app.tf("toast.default_model", "Default model: {name}", {{"name", m.name}}), "success", 2200);
                    }
                }
            }
        }
    }

    // 2. Default Thinking Effort Selector (persists defaultThinkingLevel in config.yml + live sync)
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(14.0f),
        .gap = 10.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .align_y = etch::AlignY::Center}) {
            ETCH_BOX({.axis = etch::Axis::Col, .gap = 2.0f}) {
                etch::text(app.ts("settings.models.default_thinking_heading", "DEFAULT THINKING EFFORT"), {
                    .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false
                });
                etch::text(app.ts("settings.models.default_thinking_desc",
                                  "Starting reasoning depth for new threads (saved to config.yml: defaultThinkingLevel)."), {
                    .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true
                });
            }
        }
        static constexpr const char* kDefaultLevels[] = {"off", "minimal", "low", "medium", "high", "xhigh"};
        const std::string& cur_def = app.config.omp_config().default_thinking;
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 7.0f, .wrap = true}) {
            for (int i = 0; i < 6; ++i) {
                const char* lvl = kDefaultLevels[i];
                const ThinkingLevelMeta meta = thinking_level_meta(lvl, app.i18n);
                const bool active = (cur_def == lvl) || (cur_def.empty() && std::strcmp(lvl, "high") == 0);
                const uint32_t bid = etch::id_index(etch::id("def_think_btn"), static_cast<uint32_t>(i));
                const bool hov = etch::hovered(bid);

                ETCH_BOX({
                    .id = bid,
                    .axis = etch::Axis::Row,
                    .height = etch::Sizing::fixed(ts.row_h),
                    .padding = etch::Padding::xy(11.0f, 0.0f),
                    .gap = 7.0f,
                    .align_y = etch::AlignY::Center,
                    .background = active ? (meta.cool_tint ? pal.secondary_soft : pal.selected)
                                         : (hov ? pal.hover : pal.bg_canvas),
                    .border_color = active ? (meta.bars == 0 ? pal.border_strong
                                                             : (meta.cool_tint ? pal.secondary : pal.accent_border))
                                           : pal.border_subtle,
                    .border_width = 1.0f,
                    .radius = etch::Corners::all(kRadiusSm),
                    .continuous_corners = true,
                }) {
                    render_thinking_bars(meta, pal, false);
                    etch::text(etch::str_cstr(meta.short_label), {
                        .color = active ? (meta.bars == 0 ? pal.text_primary : (meta.cool_tint ? pal.secondary : pal.accent))
                                        : pal.text_secondary,
                        .font_size_px = ts.ui_sm,
                        .font_id = active ? app.fonts.semibold : app.fonts.medium,
                        .wrap = false,
                    });
                }
                if (hov) etch::set_cursor(etch::Cursor::Hand);
                if (etch::clicked(bid)) {
                    const etch::Rect r = etch::element_bounds(bid);
                    app.config.omp_config_mut().default_thinking = lvl;
                    app.set_thinking_effort(lvl, false, {r.x + r.w * 0.5f, r.y + r.h * 0.5f});
                    if (!app.showcase_mode) {
                        std::string err;
                        if (app.config.set_omp_config_value(app.omp_path_buf, "defaultThinkingLevel", lvl, &err)) {
                            app.client.push_toast(app.tf("toast.default_thinking", "Default thinking: {level}", {{"level", meta.title}}), "success", 2200);
                        } else {
                            app.client.push_toast(err.empty() ? app.t("toast.save_failed", "Failed to save setting") : err, "error", 3200);
                        }
                    } else {
                        app.client.push_toast(app.tf("toast.default_thinking", "Default thinking: {level}", {{"level", meta.title}}), "success", 2200);
                    }
                }
            }
        }
    }

    // 3. Role Bindings Table
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(14.0f),
        .gap = 10.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        etch::text(app.ts("settings.models.roles_heading", "MODEL ROLES (config.yml)"), {
            .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false
        });
        auto role_row = [&](const char* label, const char* key, uint32_t id, char* buf, size_t cap) {
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 10.0f, .align_y = etch::AlignY::Center}) {
                etch::text(etch::str_cstr(label), {.color = pal.text_secondary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false});
                etch::box({.width = etch::Sizing::grow()});
                etch::text_input({.id = id, .buffer = buf, .capacity = static_cast<uint32_t>(cap), .placeholder = etch::str_lit("provider/model-id"),
                                  .width = etch::Sizing::fixed(276.0f), .height = etch::Sizing::fixed(ts.row_h - 2.0f), .font_size_px = ts.code - 0.5f, .focus_glow_blur = 0.0f});
                if (etch::button({.id = etch::id_index(id, 99), .label = app.ts("common.apply", "Apply"),
                                  .variant = etch::ButtonVariant::Default, .height = etch::Sizing::fixed(ts.row_h - 2.0f)})) {
                    std::string err;
                    if (app.config.set_omp_config_value(app.omp_path_buf, key, buf, &err)) {
                        app.client.push_toast(app.tf("toast.updated_key", "Updated {key}", {{"key", key}}), "success", 2400);
                    } else {
                        app.client.push_toast(err.empty() ? app.t("toast.save_failed", "Failed to save setting") : err, "error", 3600);
                    }
                }
            }
        };

        role_row(app.t("settings.models.role_default", "Default model"), "modelRoles.default", etch::id("role_def"), app.role_default_buf, sizeof(app.role_default_buf));
        role_row(app.t("settings.models.role_smol", "Smol (fast / cheap)"), "modelRoles.smol", etch::id("role_smol"), app.role_smol_buf, sizeof(app.role_smol_buf));
        role_row(app.t("settings.models.role_slow", "Slow (deep reasoning)"), "modelRoles.slow", etch::id("role_slow"), app.role_slow_buf, sizeof(app.role_slow_buf));
        role_row(app.t("settings.models.role_plan", "Plan (architecture)"), "modelRoles.plan", etch::id("role_plan"), app.role_plan_buf, sizeof(app.role_plan_buf));
    }
}

void build_appearance_tab(App& app, const Palette& pal) {
    const TypeScale ts = app.type();
    const float ls = app.is_cjk_locale() ? 0.3f : 0.8f;
    section_heading(app.t("settings.appearance.title", "Appearance & Typography"),
                    app.t("settings.appearance.subtitle", "Curated OKLCH mineral palettes, typography, and motion physics."),
                    pal, app.fonts, ts);

    // 1. 4 Architectural Palette Cards (miniature live canvas + sidebar + accent preview!)
    ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 10.0f, .wrap = true}) {
        for (uint32_t i = 0; i < kPaletteCount; ++i) {
            const Palette& cand = get_palette(static_cast<PaletteId>(i));
            const bool active = (app.config.prefs().palette == cand.id);
            const uint32_t cid = etch::id_index(etch::id("theme_swatch"), i);
            const bool hov = etch::hovered(cid);

            ETCH_BOX({
                .id = cid,
                .axis = etch::Axis::Col,
                .width = etch::Sizing::fixed(296.0f),
                .padding = etch::Padding::all(10.0f),
                .gap = 8.0f,
                .background = pal.bg_surface,
                .border_color = active ? pal.accent : (hov ? pal.border_strong : pal.border_subtle),
                .border_width = active ? 1.5f : 1.0f,
                .radius = etch::Corners::all(kRadiusMd),
                .continuous_corners = true,
                .hover_scale = 1.012f,
                .press_scale = 0.985f,
                .transition = kMotionSnap,
            }) {
                // Miniature architectural workspace preview thumbnail
                ETCH_BOX({
                    .axis = etch::Axis::Row,
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::fixed(50.0f),
                    .background = cand.bg_canvas,
                    .border_color = cand.border_default,
                    .border_width = 1.0f,
                    .radius = etch::Corners::all(7.0f),
                    .clip_children = true,
                    .continuous_corners = true,
                }) {
                    // Mini sidebar strip
                    ETCH_BOX({
                        .axis = etch::Axis::Col,
                        .width = etch::Sizing::fixed(58.0f),
                        .height = etch::Sizing::grow(),
                        .padding = etch::Padding::all(7.0f),
                        .gap = 4.0f,
                        .background = cand.bg_sidebar,
                        .border_color = cand.border_subtle,
                        .border_sides = {.right = 1.0f},
                    }) {
                        etch::box({.width = etch::Sizing::fixed(26.0f), .height = etch::Sizing::fixed(5.0f), .background = cand.accent, .radius = etch::Corners::all(3.0f)});
                        etch::box({.width = etch::Sizing::grow(), .height = etch::Sizing::fixed(4.0f), .background = cand.border_strong, .radius = etch::Corners::all(2.0f)});
                        etch::box({.width = etch::Sizing::fixed(34.0f), .height = etch::Sizing::fixed(4.0f), .background = cand.border_default, .radius = etch::Corners::all(2.0f)});
                    }
                    // Mini canvas + prompt capsule
                    ETCH_BOX({
                        .axis = etch::Axis::Col,
                        .width = etch::Sizing::grow(),
                        .height = etch::Sizing::grow(),
                        .padding = etch::Padding::all(8.0f),
                        .gap = 6.0f,
                        .justify = etch::Justify::SpaceBetween,
                    }) {
                        ETCH_BOX({.axis = etch::Axis::Row, .gap = 5.0f, .align_y = etch::AlignY::Center}) {
                            etch::box({.width = etch::Sizing::fixed(82.0f), .height = etch::Sizing::fixed(5.0f), .background = cand.text_secondary, .radius = etch::Corners::all(3.0f)});
                            etch::box({.width = etch::Sizing::fixed(32.0f), .height = etch::Sizing::fixed(5.0f), .background = cand.secondary, .radius = etch::Corners::all(3.0f)});
                        }
                        ETCH_BOX({
                            .axis = etch::Axis::Row,
                            .width = etch::Sizing::grow(),
                            .height = etch::Sizing::fixed(16.0f),
                            .padding = etch::Padding::xy(6.0f, 0.0f),
                            .align_y = etch::AlignY::Center,
                            .background = cand.bg_surface,
                            .border_color = cand.border_subtle,
                            .border_width = 1.0f,
                            .radius = etch::Corners::all(5.0f),
                        }) {
                            etch::box({.width = etch::Sizing::fixed(48.0f), .height = etch::Sizing::fixed(3.5f), .background = cand.border_strong, .radius = etch::Corners::all(2.0f)});
                            etch::box({.width = etch::Sizing::grow()});
                            etch::box({.width = etch::Sizing::fixed(10.0f), .height = etch::Sizing::fixed(10.0f), .background = cand.accent, .radius = etch::Corners::all(5.0f)});
                        }
                    }
                }

                ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .align_y = etch::AlignY::Center}) {
                    const std::string pk = palette_key(cand.id);
                    const char* pname = app.t("palette." + pk + ".name", cand.name);
                    const char* psub  = app.t("palette." + pk + ".subtitle", cand.subtitle);
                    ETCH_BOX({.axis = etch::Axis::Col, .gap = 1.0f}) {
                        etch::text(etch::str_cstr(pname), {
                            .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.semibold, .wrap = false
                        });
                        etch::text(etch::str_cstr(psub), {
                            .color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.body, .wrap = false
                        });
                    }
                    etch::box({.width = etch::Sizing::grow()});
                    if (active) {
                        etch::icon({.icon_id = app.icons.badge_check, .size_px = 15.0f, .color = pal.accent});
                    }
                }
            }
            if (hov) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(cid)) {
                app.config.prefs().palette = cand.id;
                app.apply_theme();
                app.config.save_prefs();
                const etch::Rect r = etch::element_bounds(cid);
                app.particles.spawn_point_spark({r.x + r.w * 0.5f, r.y + r.h * 0.5f}, cand.accent, 22);
            }
        }
    }

    // 2. Typography & Font Selection (live hot-swap across UI, reading prose, and code blocks)
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(14.0f),
        .gap = 12.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .align_y = etch::AlignY::Center}) {
            etch::text(app.ts("settings.appearance.fonts_heading", "TYPOGRAPHY & FONTS"), {
                .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false
            });
            etch::box({.width = etch::Sizing::grow()});
            etch::text(etch::str_cstr(app.fonts.family_label.c_str()), {
                .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.mono, .wrap = false
            });
        }

        // Interface & Reading Font
        ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 7.0f}) {
            etch::text(app.ts("settings.appearance.ui_font", "Interface & reading font"), {
                .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false
            });
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 6.0f, .wrap = true}) {
                for (size_t i = 0; i < app.ui_font_choices.size(); ++i) {
                    const auto& ch = app.ui_font_choices[i];
                    const bool active = (app.config.prefs().ui_font == ch.id) ||
                                        (ch.id.empty() && app.config.prefs().ui_font == "auto");
                    const uint32_t fid = etch::id_index(etch::id("app_ui_font"), static_cast<uint32_t>(i));
                    const bool hov = etch::hovered(fid);
                    ETCH_BOX({
                        .id = fid,
                        .axis = etch::Axis::Row,
                        .height = etch::Sizing::fixed(ts.pill_h + 1.0f),
                        .padding = etch::Padding::xy(10.0f, 0.0f),
                        .gap = 5.0f,
                        .align_y = etch::AlignY::Center,
                        .background = active ? pal.selected : (hov ? pal.hover : pal.bg_canvas),
                        .border_color = active ? pal.accent_border : pal.border_subtle,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(kRadiusSm),
                        .continuous_corners = true,
                    }) {
                        if (active) etch::icon({.icon_id = app.icons.badge_check, .size_px = 12.0f, .color = pal.accent});
                        etch::text(etch::str_cstr(ch.label.c_str()), {
                            .color = active ? pal.accent : pal.text_secondary,
                            .font_size_px = ts.ui_sm,
                            .font_id = active ? app.fonts.semibold : app.fonts.body,
                            .wrap = false,
                        });
                    }
                    if (hov) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(fid)) {
                        app.set_ui_font_pref(ch.id);
                    }
                }
            }
            // Custom system font family override (type any installed OS font name)
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 6.0f, .align_y = etch::AlignY::Center}) {
                auto cf = etch::text_input({
                    .id = etch::id("app_custom_ui_font"),
                    .buffer = app.custom_ui_font_buf,
                    .capacity = sizeof(app.custom_ui_font_buf),
                    .placeholder = app.ts("settings.appearance.custom_font_placeholder", "Or enter any installed system font name..."),
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::fixed(ts.pill_h + 2.0f),
                    .padding = etch::Padding::xy(10.0f, 4.0f),
                    .font_size_px = ts.ui_sm,
                    .background = pal.bg_input,
                    .border_color = pal.border_default,
                    .focus_border_color = pal.accent_border,
                    .radius = etch::Corners::all(kRadiusSm),
                    .submit_on_enter = true,
                    .focus_glow_blur = 0.0f,
                });
                const bool apply_clicked = etch::button({
                    .id = etch::id("app_custom_ui_font_apply"),
                    .label = app.ts("common.apply", "Apply"),
                    .variant = etch::ButtonVariant::Default,
                    .height = etch::Sizing::fixed(ts.pill_h + 2.0f),
                });
                if (cf.submitted || apply_clicked) {
                    app.set_ui_font_pref(app.custom_ui_font_buf);
                }
            }
        }

        etch::box({.width = etch::Sizing::grow(), .height = etch::Sizing::fixed(1.0f), .background = pal.border_subtle});

        // Code & Monospace Font
        ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 7.0f}) {
            etch::text(app.ts("settings.appearance.mono_font", "Code & monospace font"), {
                .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false
            });
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 6.0f, .wrap = true}) {
                for (size_t i = 0; i < app.mono_font_choices.size(); ++i) {
                    const auto& ch = app.mono_font_choices[i];
                    const bool active = (app.config.prefs().mono_font == ch.id) ||
                                        (ch.id.empty() && app.config.prefs().mono_font == "auto");
                    const uint32_t mid = etch::id_index(etch::id("app_mono_font"), static_cast<uint32_t>(i));
                    const bool hov = etch::hovered(mid);
                    ETCH_BOX({
                        .id = mid,
                        .axis = etch::Axis::Row,
                        .height = etch::Sizing::fixed(ts.pill_h + 1.0f),
                        .padding = etch::Padding::xy(10.0f, 0.0f),
                        .gap = 5.0f,
                        .align_y = etch::AlignY::Center,
                        .background = active ? pal.selected : (hov ? pal.hover : pal.bg_canvas),
                        .border_color = active ? pal.accent_border : pal.border_subtle,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(kRadiusSm),
                        .continuous_corners = true,
                    }) {
                        if (active) etch::icon({.icon_id = app.icons.badge_check, .size_px = 12.0f, .color = pal.accent});
                        etch::text(etch::str_cstr(ch.label.c_str()), {
                            .color = active ? pal.accent : pal.text_secondary,
                            .font_size_px = ts.code - 0.5f,
                            .font_id = app.fonts.mono,
                            .wrap = false,
                        });
                    }
                    if (hov) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(mid)) {
                        app.set_mono_font_pref(ch.id);
                    }
                }
            }
        }
    }

    // 3. Motion & Window Surface
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(14.0f),
        .gap = 12.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 12.0f, .align_y = etch::AlignY::Center}) {
            ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 2.0f, .shrink_width = true}) {
                etch::text(app.ts("settings.appearance.particles_title", "GPU Particle Physics"), {.color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false});
                etch::text(app.ts("settings.appearance.particles_desc", "Subtle send bursts, dismissals & welcome ambient starlight."), {.color = pal.text_muted, .font_size_px = ts.ui_sm, .wrap = true});
            }
            const char* pnames[3] = {
                app.t("settings.appearance.particles_off", "Off"),
                app.t("settings.appearance.particles_subtle", "Subtle"),
                app.t("settings.appearance.particles_signature", "Signature"),
            };
            ETCH_BOX({.axis = etch::Axis::Row, .padding = etch::Padding::all(2.0f), .gap = 2.0f, .background = pal.bg_input, .radius = etch::Corners::all(kRadiusSm)}) {
                for (uint32_t i = 0; i < 3; ++i) {
                    const uint32_t pid = etch::id_index(etch::id("p_dens"), i);
                    const bool active = (static_cast<uint32_t>(app.config.prefs().particle_density) == i);
                    ETCH_BOX({
                        .id = pid,
                        .padding = etch::Padding::xy(10.0f, 5.0f),
                        .background = active ? pal.bg_elevated : etch::Color::transparent(),
                        .border_color = active ? pal.border_subtle : etch::Color::transparent(),
                        .border_width = active ? 1.0f : 0.0f,
                        .radius = etch::Corners::all(6.0f),
                    }) {
                        etch::text(etch::str_cstr(pnames[i]), {
                            .color = active ? pal.accent : pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = false
                        });
                    }
                    if (etch::hovered(pid)) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(pid)) {
                        app.config.prefs().particle_density = static_cast<ParticleDensity>(i);
                        app.apply_theme();
                        app.config.save_prefs();
                    }
                }
            }
        }

        etch::box({.width = etch::Sizing::grow(), .height = etch::Sizing::fixed(1.0f), .background = pal.border_subtle});

        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .align_y = etch::AlignY::Center}) {
            ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 1.0f}) {
                etch::text(app.ts("settings.appearance.reduced_motion_title", "Reduced motion"), {.color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false});
                etch::text(app.ts("settings.appearance.reduced_motion_desc", "Settle springs and shaders immediately with zero continuous particle motion."), {.color = pal.text_muted, .font_size_px = ts.ui_sm, .wrap = true});
            }
            if (etch::toggle_switch({.id = etch::id("set_reduced_motion"), .value = &app.config.prefs().reduced_motion,
                                     .on_color = pal.accent, .off_color = pal.bg_input, .border_color = pal.border_default})) {
                app.apply_theme();
                app.config.save_prefs();
            }
        }
#ifdef _WIN32
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .align_y = etch::AlignY::Center}) {
            ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 1.0f}) {
                etch::text(app.ts("settings.appearance.mica_title", "Windows Mica backdrop"), {.color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false});
                etch::text(app.ts("settings.appearance.mica_desc", "System translucent desktop surface (applies on next launch)."), {.color = pal.text_muted, .font_size_px = ts.ui_sm, .wrap = true});
            }
            if (etch::toggle_switch({.id = etch::id("set_mica"), .value = &app.config.prefs().mica_backdrop,
                                     .on_color = pal.accent, .off_color = pal.bg_input, .border_color = pal.border_default})) {
                app.config.save_prefs();
            }
        }
#endif
    }
}

void build_safety_tab(App& app, const Palette& pal) {
    const TypeScale ts = app.type();
    section_heading(app.t("settings.safety.title", "Tools & Approval Policy"),
                    app.t("settings.safety.subtitle", "Controls when OMP pauses for confirmation before modifying files or running shell commands."),
                    pal, app.fonts, ts);

    struct ModeCard { const char* id; const char* title; const char* tag; const char* desc; uint16_t ico; bool caution; };
    const ModeCard modes[4] = {
        {"always-ask",
         app.t("settings.safety.always_ask.title", "Always Ask"),
         app.t("settings.safety.always_ask.tag", "Strict"),
         app.t("settings.safety.always_ask.desc", "Auto-approves read-only tools; prompts before every write or shell command."),
         app.icons.lock_shield, false},
        {"always-ask-cached",
         app.t("settings.safety.cached.title", "Ask & Remember"),
         app.t("settings.safety.cached.tag", "Recommended"),
         app.t("settings.safety.cached.desc", "Prompts on writes with option to remember per file or folder in approval-cache.db."),
         app.icons.shield_check, false},
        {"write",
         app.t("settings.safety.write.title", "Auto-Write, Ask Shell"),
         app.t("settings.safety.write.tag", "Fast"),
         app.t("settings.safety.write.desc", "Allows file edits freely; prompts before executing bash / terminal commands."),
         app.icons.file_pen, false},
        {"yolo",
         app.t("settings.safety.yolo.title", "Autonomous (YOLO)"),
         app.t("settings.safety.yolo.tag", "Unrestricted"),
         app.t("settings.safety.yolo.desc", "Runs all tools autonomously except explicitly restricted folders or per-tool overrides."),
         app.icons.zap_warning, true},
    };

    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 8.0f}) {
        for (uint32_t i = 0; i < 4; ++i) {
            const bool active = (app.config.omp_config().approval_mode == modes[i].id);
            const uint32_t cid = etch::id_index(etch::id("appr_mode"), i);
            const bool hov = etch::hovered(cid);
            ETCH_BOX({
                .id = cid,
                .axis = etch::Axis::Row,
                .width = etch::Sizing::grow(),
                .padding = etch::Padding::all(12.0f),
                .gap = 12.0f,
                .align_y = etch::AlignY::Center,
                .background = active ? pal.active : (hov ? pal.bg_elevated : pal.bg_surface),
                .border_color = active ? (modes[i].caution ? pal.warning : pal.accent) : pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusMd),
                .continuous_corners = true,
            }) {
                etch::icon({
                    .icon_id = modes[i].ico,
                    .size_px = 16.5f,
                    .color = active ? (modes[i].caution ? pal.warning : pal.accent) : pal.text_muted,
                });
                ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 2.0f}) {
                    ETCH_BOX({.axis = etch::Axis::Row, .gap = 8.0f, .align_y = etch::AlignY::Center}) {
                        etch::text(etch::str_cstr(modes[i].title), {.color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.semibold, .wrap = false});
                        ETCH_BOX({
                            .padding = etch::Padding::xy(6.0f, 1.5f),
                            .background = modes[i].caution ? pal.warning_soft : pal.bg_canvas,
                            .border_color = modes[i].caution ? pal.warning : pal.border_subtle,
                            .border_width = 1.0f,
                            .radius = etch::Corners::all(kRadiusPill),
                        }) {
                            etch::text(etch::str_cstr(modes[i].tag), {
                                .color = modes[i].caution ? pal.warning : pal.text_muted,
                                .font_size_px = ts.micro, .font_id = app.fonts.mono, .wrap = false
                            });
                        }
                    }
                    etch::text(etch::str_cstr(modes[i].desc), {.color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true});
                }
                if (active) etch::icon({.icon_id = app.icons.badge_check, .size_px = 15.0f, .color = pal.accent});
            }
            if (hov) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(cid)) {
                std::string err;
                if (app.config.set_omp_config_value(app.omp_path_buf, "tools.approvalMode", modes[i].id, &err)) {
                    app.client.push_toast(app.tf("toast.approval_mode", "Approval mode: {mode}", {{"mode", modes[i].title}}), "success", 2400);
                } else {
                    app.client.push_toast(err.empty() ? app.t("toast.save_failed", "Failed to save setting") : err, "error", 3600);
                }
            }
        }
    }
}

void build_ecosystem_tab(App& app, const Palette& pal) {
    const TypeScale ts = app.type();
    const float ls = app.is_cjk_locale() ? 0.3f : 0.8f;
    section_heading(app.t("settings.ecosystem.title", "MCP, Extensions & Skills"),
                    app.t("settings.ecosystem.subtitle", "Manage MCP servers in mcp.json and inspect discovered OMP plugins."),
                    pal, app.fonts, ts);

    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(12.0f),
        .gap = 8.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .align_y = etch::AlignY::Center}) {
            etch::text(app.ts("settings.ecosystem.mcp_heading", "MCP SERVERS"), {.color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false});
            etch::box({.width = etch::Sizing::grow()});
            if (etch::button({.id = etch::id("mcp_add_toggle"), .label = app.mcp_form.open ? app.ts("common.cancel", "Cancel") : app.ts("settings.ecosystem.add_mcp", "Add MCP Server"),
                              .icon_id = app.mcp_form.open ? app.icons.close : app.icons.plus_circle,
                              .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(26.0f)})) {
                app.mcp_form.open = !app.mcp_form.open;
            }
        }

        if (app.mcp_form.open) {
            auto& mf = app.mcp_form;
            const etch::Str transports[] = {etch::str_lit("stdio"), etch::str_lit("http"), etch::str_lit("sse")};
            ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .padding = etch::Padding::all(10.0f), .gap = 8.0f,
                      .background = pal.bg_canvas, .border_color = pal.border_default, .border_width = 1.0f, .radius = etch::Corners::all(kRadiusSm)}) {
                ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f}) {
                    etch::text_input({.id = etch::id("mf_name"), .buffer = mf.name, .capacity = sizeof(mf.name),
                                      .placeholder = app.ts("settings.ecosystem.server_name", "Server name"), .width = etch::Sizing::fixed(160.0f), .height = etch::Sizing::fixed(ts.row_h - 2.0f),
                                      .font_size_px = ts.ui_sm, .focus_glow_blur = 0.0f});
                    etch::combo_box({.id = etch::id("mf_tr"), .selected_index = &mf.transport_index, .items = transports,
                                     .width = etch::Sizing::fixed(96.0f), .height_px = ts.row_h - 2.0f, .border_color = pal.border_default});
                    if (mf.transport_index == 0) {
                        etch::text_input({.id = etch::id("mf_cmd"), .buffer = mf.command, .capacity = sizeof(mf.command),
                                          .placeholder = etch::str_lit("npx -y @modelcontextprotocol/server-..."), .height = etch::Sizing::fixed(ts.row_h - 2.0f),
                                          .font_size_px = ts.ui_sm, .focus_glow_blur = 0.0f});
                    } else {
                        etch::text_input({.id = etch::id("mf_url"), .buffer = mf.url, .capacity = sizeof(mf.url),
                                          .placeholder = etch::str_lit("https://mcp.example.com/sse"), .height = etch::Sizing::fixed(ts.row_h - 2.0f),
                                          .font_size_px = ts.ui_sm, .focus_glow_blur = 0.0f});
                    }
                }
                if (etch::button({.id = etch::id("mf_save"), .label = app.ts("settings.ecosystem.save_mcp", "Save to mcp.json"),
                                  .icon_id = app.icons.check, .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(28.0f)})) {
                    McpServerConfig srv;
                    srv.name = mf.name;
                    srv.transport = (mf.transport_index == 0) ? "stdio" : (mf.transport_index == 1 ? "http" : "sse");
                    if (mf.transport_index == 0) {
                        std::istringstream iss(mf.command);
                        std::string tok;
                        if (iss >> tok) srv.command = tok;
                        while (iss >> tok) srv.args.push_back(tok);
                    } else {
                        srv.url = mf.url;
                    }
                    std::string err;
                    if (app.config.upsert_mcp_server(srv, &err)) {
                        mf.open = false;
                        app.client.push_toast(app.tf("toast.added_mcp", "Added MCP server '{name}'", {{"name", srv.name}}), "success", 2800);
                    } else {
                        mf.error_msg = err;
                    }
                }
            }
        }

        if (app.config.mcp_servers().empty()) {
            etch::text(app.ts("settings.ecosystem.mcp_empty", "No MCP servers configured in ~/.omp/agent/mcp.json."), {.color = pal.text_muted, .font_size_px = ts.ui_sm, .wrap = true});
        }
        std::string del_mcp;
        for (size_t i = 0; i < app.config.mcp_servers().size(); ++i) {
            const auto& s = app.config.mcp_servers()[i];
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .padding = etch::Padding::xy(10.0f, 6.0f),
                      .gap = 8.0f, .align_y = etch::AlignY::Center, .background = pal.bg_canvas, .radius = etch::Corners::all(6.0f)}) {
                etch::icon({.icon_id = app.icons.plug_connected, .size_px = 13.0f, .color = pal.accent});
                etch::text(etch::str_cstr(s.name.c_str()), {.color = pal.text_primary, .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = false});
                etch::text(etch::str_cstr((s.transport + " · " + s.scope).c_str()), {.color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false});
                etch::box({.width = etch::Sizing::grow()});
                if (s.scope == "user") {
                    if (etch::button({.id = etch::id_index(etch::id("mcp_del"), static_cast<uint32_t>(i)),
                                      .label = app.ts("common.remove", "Remove"), .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(22.0f)})) {
                        del_mcp = s.name;
                    }
                }
            }
        }
        if (!del_mcp.empty()) app.config.delete_mcp_server(del_mcp);
    }

    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(12.0f),
        .gap = 6.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .align_y = etch::AlignY::Center}) {
            etch::text(app.ts("settings.ecosystem.discovered_heading", "DISCOVERED EXTENSIONS, SKILLS & RULES"),
                       {.color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = ls, .wrap = false});
            etch::box({.width = etch::Sizing::grow()});
            if (etch::button({.id = etch::id("eco_open_dir"), .label = app.ts("settings.ecosystem.open_agent_dir", "Open ~/.omp/agent"),
                              .icon_id = app.icons.external,
                              .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(24.0f)})) {
                reveal_in_file_manager(app.config.agent_dir());
            }
        }
        if (app.config.ecosystem_items().empty()) {
            etch::text(app.ts("settings.ecosystem.discovered_empty",
                              "No custom extensions, skills, or rules discovered in current workspace or ~/.omp/agent."),
                       {.color = pal.text_muted, .font_size_px = ts.ui_sm, .wrap = true});
        }
        for (const auto& item : app.config.ecosystem_items()) {
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .padding = etch::Padding::xy(8.0f, 4.0f), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
                etch::text(etch::str_cstr(item.kind.c_str()), {.color = pal.accent, .font_size_px = ts.micro, .font_id = app.fonts.mono, .wrap = false});
                etch::text(etch::str_cstr(item.name.c_str()), {.color = pal.text_primary, .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = false});
                etch::box({.width = etch::Sizing::grow()});
                etch::text(etch::str_cstr(item.scope.c_str()), {.color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false});
            }
        }
    }
}

void build_engine_and_logs_tab(App& app, const Palette& pal) {
    const TypeScale ts = app.type();
    section_heading(app.t("settings.engine.title", "Engine & Wire Diagnostics"),
                    app.t("settings.engine.subtitle", "Configure OMP runtime binary, profile, and inspect live RPC v2 framing."),
                    pal, app.fonts, ts);

    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(14.0f),
        .gap = 10.0f,
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        etch::text(app.ts("settings.engine.exe_path", "OMP Executable Path"), {.color = pal.text_muted, .font_size_px = ts.meta, .wrap = false});
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f}) {
            etch::text_input({.id = etch::id("eng_omp_path"), .buffer = app.omp_path_buf, .capacity = sizeof(app.omp_path_buf),
                              .placeholder = app.ts("settings.engine.exe_placeholder", "Auto-detect from PATH"), .height = etch::Sizing::fixed(ts.row_h - 2.0f),
                              .font_size_px = ts.ui_sm, .focus_glow_blur = 0.0f});
            if (etch::button({.id = etch::id("eng_browse"), .label = app.ts("settings.engine.browse", "Browse"), .height = etch::Sizing::fixed(ts.row_h - 2.0f)}) && !app.headless_mode) {
                etch::open_file_dialog(app.ctx, app.window, {}, nullptr, kDialogPickOmpExe, false);
            }
            if (etch::button({.id = etch::id("eng_detect"), .label = app.ts("settings.engine.auto_detect", "Auto-Detect"), .height = etch::Sizing::fixed(ts.row_h - 2.0f)})) {
                const std::string found = find_omp_executable("");
                fill_buf(app.omp_path_buf, sizeof(app.omp_path_buf), found);
                app.omp_version_label = query_omp_version(found);
            }
        }

        etch::text(app.ts("settings.engine.profile_label", "Profile (optional, empty = default ~/.omp)"), {.color = pal.text_muted, .font_size_px = ts.meta, .wrap = false});
        etch::text_input({.id = etch::id("eng_profile"), .buffer = app.profile_buf, .capacity = sizeof(app.profile_buf),
                          .placeholder = etch::str_lit("default"), .width = etch::Sizing::fixed(240.0f), .height = etch::Sizing::fixed(ts.row_h - 2.0f),
                          .font_size_px = ts.ui_sm, .focus_glow_blur = 0.0f});

        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
            if (etch::button({.id = etch::id("eng_save_reconnect"), .label = app.ts("settings.engine.save_restart", "Save & Restart OMP"),
                              .icon_id = app.icons.refresh, .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(ts.row_h - 2.0f)})) {
                app.config.prefs().omp_executable = app.omp_path_buf;
                app.config.prefs().profile = app.profile_buf;
                app.config.save_prefs();
                app.omp_version_label = query_omp_version(app.omp_path_buf);
                app.config.reload_all(app.config.prefs().active_workspace);
                if (!app.showcase_mode) app.start_omp_for_workspace(app.config.prefs().active_workspace);
                app.client.push_toast(app.t("toast.engine_restarted", "OMP engine restarted"), "success", 2400);
            }
            etch::box({.width = etch::Sizing::grow()});
            const std::string det = app.tf("settings.engine.detected", "Detected: {ver}", {{"ver", app.omp_version_label}});
            etch::text(etch::str_cstr(det.c_str()), {
                .color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
            });
        }
    }

    // Compact live RPC v2 wire trace right below engine config
    const std::string header = app.tf("settings.engine.wire_stats",
        "PID {pid}  ·  Protocol v{proto}  ·  GPU {gpu}  ·  {frames} frames logged", {
            {"pid", std::to_string(app.client.process_pid())},
            {"proto", std::to_string(app.client.negotiated_version())},
            {"gpu", app.gpu_driver_label},
            {"frames", std::to_string(app.client.rpc_log().size())},
        });
    etch::text(etch::str_cstr(header.c_str()), {.color = pal.accent, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false});

    ETCH_BOX({
        .id = etch::id("diag_log_scroll"),
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::fixed(210.0f),
        .padding = etch::Padding::all(10.0f),
        .gap = 3.0f,
        .background = pal.bg_code,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .clip_children = true,
        .scroll_y = true,
        .continuous_corners = true,
    }) {
        const auto& logs = app.client.rpc_log();
        if (logs.empty()) {
            etch::text(app.ts("settings.engine.no_logs", "No live RPC frames logged yet (Showcase or disconnected)."), {
                .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
            });
        }
        for (auto it = logs.rbegin(); it != logs.rend(); ++it) {
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
                etch::text(it->outbound ? etch::str_lit("OUT") : etch::str_lit("IN "), {
                    .color = it->outbound ? pal.accent : pal.secondary, .font_size_px = ts.micro, .font_id = app.fonts.mono_tnum, .wrap = false
                });
                etch::text(etch::str_cstr(it->frame_type.c_str()), {
                    .color = pal.text_primary, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                });
                etch::text(etch::str_cstr(it->summary.c_str()), {
                    .color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false, .ellipsis = true
                });
            }
        }
    }
}

} // namespace

void build_settings_view(App& app) {
    const Palette& pal = app.palette();
    const uint32_t scrim_id = etch::id("settings_scrim");
    const uint32_t sheet_id = etch::id("settings_sheet");

    const float modal_w = std::clamp(app.viewport.x - 80.0f, 720.0f, 920.0f);
    const float modal_h = std::clamp(app.viewport.y - 72.0f, 540.0f, 668.0f);

    ETCH_BOX({
        .id = scrim_id,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::grow(),
        .align_x = etch::AlignX::Center,
        .align_y = etch::AlignY::Center,
        .background = pal.scrim,
        .attach_to = etch::AttachTo::Root,
        .anchor_element = etch::Anchor::TopLeft,
        .anchor_parent = etch::Anchor::TopLeft,
        .z_index = 230,
        .backdrop_blur = 8.0f,
    }) {
        ETCH_BOX({
            .id = sheet_id,
            .axis = etch::Axis::Row,
            .width = etch::Sizing::fixed(modal_w),
            .height = etch::Sizing::fixed(modal_h),
            .background = pal.bg_canvas,
            .border_color = pal.border_strong,
            .border_width = 1.0f,
            .radius = etch::Corners::all(kRadiusLg),
            .shadow_color = pal.shadow,
            .shadow_blur = 42.0f,
            .shadow_offset = {0.0f, 18.0f},
            .clip_children = true,
            .continuous_corners = true,
            .top_rim_alpha = pal.dark ? 0.22f : 0.0f,
        }) {
            // Priority-ordered 2-Group Navigation Rail (Linear/Raycast calm neutral active elevation)
            const TypeScale ts = app.type();
            ETCH_BOX({
                .axis = etch::Axis::Col,
                .width = etch::Sizing::fixed(212.0f),
                .height = etch::Sizing::grow(),
                .padding = etch::Padding::all(12.0f),
                .gap = 2.0f,
                .background = pal.bg_sidebar,
                .border_color = pal.border_subtle,
                .border_sides = {.right = 1.0f},
            }) {
                etch::text(app.ts("settings.title", "Settings"), {
                    .color = pal.text_primary, .font_size_px = ts.h2, .font_id = app.fonts.semibold, .wrap = false
                });

                struct NavItem { SettingsTab tab; const char* key; const char* fallback; uint16_t idle; uint16_t act; };
                const NavItem core_items[4] = {
                    {SettingsTab::General,    "settings.nav.general",    "General",    app.icons.sliders, app.icons.sliders_active},
                    {SettingsTab::Providers,  "settings.nav.providers",  "Providers",  app.icons.key,     app.icons.key_active},
                    {SettingsTab::Models,     "settings.nav.models",     "Models",     app.icons.cpu,     app.icons.cpu_active},
                    {SettingsTab::Appearance, "settings.nav.appearance", "Appearance", app.icons.palette, app.icons.palette_active},
                };
                const NavItem sys_items[3] = {
                    {SettingsTab::Safety,     "settings.nav.safety",     "Safety",        app.icons.shield,   app.icons.shield_active},
                    {SettingsTab::Ecosystem,  "settings.nav.ecosystem",  "MCP & Skills",  app.icons.blocks,   app.icons.blocks_active},
                    {SettingsTab::Engine,     "settings.nav.engine",     "Engine & Logs", app.icons.activity, app.icons.activity_active},
                };

                auto render_group = [&](const char* heading, const NavItem* items, size_t count, float top_m) {
                    ETCH_BOX({.padding = {.left = 9.0f, .top = top_m, .bottom = 4.0f}}) {
                        etch::text(etch::str_cstr(heading), {
                            .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold,
                            .letter_spacing = app.is_cjk_locale() ? 0.3f : 0.9f, .wrap = false
                        });
                    }
                    for (size_t i = 0; i < count; ++i) {
                        const uint32_t nid = etch::id_index(etch::id("set_nav"), static_cast<uint32_t>(items[i].tab));
                        const bool active = (app.settings_tab == items[i].tab);
                        const bool hov = etch::hovered(nid);
                        const etch::Color nav_bg = active
                            ? (pal.dark ? etch::Color::rgba8(244, 238, 226, 17) : etch::Color::rgba8(28, 25, 21, 14))
                            : (hov ? pal.hover : etch::Color::transparent());
                        ETCH_BOX({
                            .id = nid,
                            .axis = etch::Axis::Row,
                            .width = etch::Sizing::grow(),
                            .height = etch::Sizing::fixed(ts.row_h),
                            .padding = etch::Padding::xy(9.0f, 0.0f),
                            .gap = 8.0f,
                            .align_y = etch::AlignY::Center,
                            .background = nav_bg,
                            .border_color = active ? pal.border_subtle : etch::Color::transparent(),
                            .border_width = active ? 1.0f : 0.0f,
                            .radius = etch::Corners::all(kRadiusSm),
                            .continuous_corners = true,
                        }) {
                            etch::icon({
                                .icon_id = active ? items[i].act : items[i].idle,
                                .size_px = 14.5f,
                                .color = active ? pal.accent : (hov ? pal.text_primary : pal.text_muted),
                            });
                            etch::text(app.ts(items[i].key, items[i].fallback), {
                                .color = active ? pal.text_primary : (hov ? pal.text_primary : pal.text_secondary),
                                .font_size_px = ts.ui, .font_id = active ? app.fonts.medium : app.fonts.body, .wrap = false
                            });
                        }
                        if (hov) etch::set_cursor(etch::Cursor::Hand);
                        if (etch::clicked(nid)) {
                            app.settings_tab = items[i].tab;
                        }
                    }
                };

                render_group(app.t("settings.group.core", "CORE"), core_items, 4, 10.0f);
                render_group(app.t("settings.group.system", "SYSTEM"), sys_items, 3, 14.0f);

                etch::box({.height = etch::Sizing::grow()});

                if (etch::button({.id = etch::id("set_close_btn"), .label = app.ts("common.done", "Done"),
                                  .variant = etch::ButtonVariant::Default, .width = etch::Sizing::grow(), .height = etch::Sizing::fixed(30.0f)})) {
                    app.settings_open = false;
                }
            }

            // Right scrollable content canvas
            ETCH_BOX({
                .id = etch::id("set_content_scroll"),
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::grow(),
                .padding = etch::Padding::all(22.0f),
                .gap = 14.0f,
                .clip_children = true,
                .scroll_y = true,
                .shrink_width = true,
                .shrink_height = true,
                .scroll_fade_px = 18.0f,
            }) {
                switch (app.settings_tab) {
                    case SettingsTab::General:    build_general_tab(app, pal); break;
                    case SettingsTab::Providers:  build_providers_tab(app, pal); break;
                    case SettingsTab::Models:     build_models_tab(app, pal); break;
                    case SettingsTab::Appearance: build_appearance_tab(app, pal); break;
                    case SettingsTab::Safety:     build_safety_tab(app, pal); break;
                    case SettingsTab::Ecosystem:  build_ecosystem_tab(app, pal); break;
                    case SettingsTab::Engine:     build_engine_and_logs_tab(app, pal); break;
                }
            }
        }
    }

    if (etch::clicked(scrim_id) && !etch::hovered(sheet_id)) {
        app.settings_open = false;
    }
}

} // namespace ompe
