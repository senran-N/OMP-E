#include "ui/app.h"
#include "core/platform.h"

#include <cstdio>

namespace ompe {

void build_about_modal(App& app) {
    const Palette& pal = app.palette();
    const TypeScale ts = app.type();
    const PlatformInfo pi = get_platform_info();

    const uint32_t scrim_id = etch::id("about_scrim");
    const uint32_t card_id = etch::id("about_card");

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
        .z_index = 235,
        .backdrop_blur = 8.0f,
    }) {
        ETCH_BOX({
            .id = card_id,
            .axis = etch::Axis::Col,
            .width = etch::Sizing::fixed(484.0f),
            .padding = etch::Padding::all(24.0f),
            .gap = 16.0f,
            .background = pal.bg_elevated,
            .border_color = pal.border_strong,
            .border_width = 1.0f,
            .radius = etch::Corners::all(kRadiusLg),
            .shadow_color = pal.shadow,
            .shadow_blur = 40.0f,
            .shadow_offset = {0.0f, 16.0f},
            .continuous_corners = true,
            .top_rim_alpha = pal.dark ? 0.24f : 0.0f,
        }) {
            // Editorial Serif Brand Masthead (frameless sculpted crest + Instrument Serif OMP + hairline rule)
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 12.0f, .align_y = etch::AlignY::Center}) {
                etch::icon({.icon_id = app.icons.logo, .size_px = 24.0f, .color = pal.accent});
                etch::text(etch::str_lit("OMP"), {
                    .color = pal.text_primary,
                    .font_size_px = ts.display + 6.0f,
                    .font_id = app.fonts.serif,
                    .letter_spacing = 1.6f,
                    .wrap = false,
                });
                etch::box({
                    .width = etch::Sizing::fixed(1.0f),
                    .height = etch::Sizing::fixed(26.0f),
                    .background = pal.accent_border,
                });
                ETCH_BOX({.axis = etch::Axis::Col, .gap = 2.0f}) {
                    etch::text(app.ts("about.subtitle_title", "Desktop Studio"), {
                        .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.semibold, .wrap = false
                    });
                    const std::string ver_str = app.tf("about.version_line", "Pure-GPU Native Client · v{ver}", {{"ver", OMPE_VERSION_STRING}});
                    etch::text(etch::str_cstr(ver_str.c_str()), {
                        .color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                    });
                }
                etch::box({.width = etch::Sizing::grow()});
                const uint32_t cid = etch::id("about_close_btn");
                const bool ch = etch::hovered(cid);
                ETCH_BOX({
                    .id = cid,
                    .width = etch::Sizing::fixed(28.0f),
                    .height = etch::Sizing::fixed(28.0f),
                    .align_x = etch::AlignX::Center,
                    .align_y = etch::AlignY::Center,
                    .background = ch ? pal.hover : etch::Color::transparent(),
                    .radius = etch::Corners::all(7.0f),
                }) {
                    etch::icon({.icon_id = app.icons.close, .size_px = 13.0f, .color = pal.text_muted});
                }
                if (ch) etch::set_cursor(etch::Cursor::Hand);
                if (etch::clicked(cid)) app.about_open = false;
            }

            // Crisp technical spec sheet (tabular mono alignment)
            ETCH_BOX({
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .padding = etch::Padding::all(12.0f),
                .gap = 7.0f,
                .background = pal.bg_surface,
                .border_color = pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusMd),
                .continuous_corners = true,
            }) {
                auto row = [&](const char* k, const std::string& v) {
                    ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 12.0f, .align_y = etch::AlignY::Center}) {
                        etch::text(etch::str_cstr(k), {
                            .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = false
                        });
                        etch::box({.width = etch::Sizing::grow()});
                        ETCH_BOX({.width = etch::Sizing::fit(), .clip_children = true, .shrink_width = true}) {
                            etch::text(etch::str_cstr(v.c_str()), {
                                .color = pal.text_primary, .font_size_px = ts.code - 0.5f, .font_id = app.fonts.mono_tnum, .wrap = false, .ellipsis = true
                            });
                        }
                    }
                };
                row(app.t("about.row.engine", "OMP Engine"), app.omp_version_label.empty() ? app.t("about.not_detected", "Not detected") : app.omp_version_label);
                row(app.t("about.row.rpc", "RPC Protocol"), app.tf("about.rpc_val", "v{ver} · delta stream", {{"ver", std::to_string(app.client.negotiated_version())}}));
                row(app.t("about.row.renderer", "UI Renderer"), "Etch " ETCH_VERSION_STRING " (" + app.gpu_driver_label + ")");
                row(app.t("about.row.doc_engine", "Document Engine"), "Inlay " INLAY_VERSION_STRING " (Markdown / Math / Mermaid)");
                row(app.t("about.row.typography", "Typography"), app.fonts.family_label.empty() ? "System UI" : app.fonts.family_label);
                row(app.t("about.row.platform", "Platform"), pi.os_name + " · " + pi.arch);
            }

            // Software Update strip (GitHub Releases SemVer checker + native installer download)
            {
                const auto upd = app.updater.snapshot();
                ETCH_BOX({
                    .axis = etch::Axis::Col,
                    .width = etch::Sizing::grow(),
                    .padding = etch::Padding::all(12.0f),
                    .gap = 8.0f,
                    .background = (upd.state == UpdateState::Available || upd.state == UpdateState::ReadyToInstall)
                        ? pal.selected_subtle : pal.bg_surface,
                    .border_color = (upd.state == UpdateState::Available || upd.state == UpdateState::ReadyToInstall)
                        ? pal.accent_border : pal.border_subtle,
                    .border_width = 1.0f,
                    .radius = etch::Corners::all(kRadiusMd),
                    .continuous_corners = true,
                }) {
                    ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
                        std::string st_txt;
                        etch::Color st_col = pal.text_secondary;
                        if (upd.state == UpdateState::Checking) {
                            st_txt = app.t("update.checking", "Checking GitHub Releases...");
                            st_col = pal.accent;
                        } else if (upd.state == UpdateState::UpToDate) {
                            st_txt = app.tf("update.up_to_date", "Up to date (v{ver})", {{"ver", OMPE_VERSION_STRING}});
                            st_col = pal.success;
                        } else if (upd.state == UpdateState::Available) {
                            st_txt = app.tf("update.available", "Version v{ver} is available", {{"ver", upd.latest_version}});
                            st_col = pal.accent;
                        } else if (upd.state == UpdateState::Downloading) {
                            char pct[16];
                            std::snprintf(pct, sizeof(pct), "%d", static_cast<int>(upd.download_progress * 100.0f));
                            st_txt = app.tf("update.downloading", "Downloading installer ({pct}%)...", {{"pct", pct}});
                            st_col = pal.accent;
                        } else if (upd.state == UpdateState::ReadyToInstall) {
                            st_txt = upd.sha256_verified
                                ? app.t("update.verified_sha256", "Installer ready · SHA-256 verified")
                                : app.t("update.ready", "Installer ready");
                            st_col = pal.success;
                        } else if (upd.state == UpdateState::Error) {
                            st_txt = upd.error_message;
                            st_col = pal.text_muted;
                        } else {
                            st_txt = app.tf("update.current_ver", "Installed · v{ver}", {{"ver", OMPE_VERSION_STRING}});
                        }

                        if (upd.state == UpdateState::Checking || upd.state == UpdateState::Downloading) {
                            etch::icon({.icon_id = app.icons.loader_arc, .size_px = 13.0f, .color = pal.accent, .rotation_rad = app.elapsed_s * 5.2f});
                        } else if (upd.state == UpdateState::UpToDate || upd.state == UpdateState::ReadyToInstall) {
                            etch::icon({.icon_id = app.icons.badge_check, .size_px = 13.5f, .color = pal.success});
                        } else if (upd.state == UpdateState::Available) {
                            etch::icon({.icon_id = app.icons.arrow_down, .size_px = 13.0f, .color = pal.accent});
                        }

                        ETCH_BOX({.width = etch::Sizing::grow(), .shrink_width = true}) {
                            etch::text(etch::str_cstr(st_txt.c_str()), {
                                .color = st_col, .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = false, .ellipsis = true
                            });
                        }

                        if (upd.state == UpdateState::Available) {
                            if (etch::button({.id = etch::id("abt_upd_dl"), .label = app.ts("update.download", "Download Installer"),
                                              .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(26.0f)})) {
                                app.updater.download_installer();
                            }
                        } else if (upd.state == UpdateState::ReadyToInstall) {
                            if (etch::button({.id = etch::id("abt_upd_inst"), .label = app.ts("update.install_now", "Install Now"),
                                              .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(26.0f)})) {
                                std::string err;
                                if (!app.updater.launch_installer(&err)) {
                                    app.client.push_toast(err, "error", 3200);
                                }
                            }
                        } else {
                            const bool busy = (upd.state == UpdateState::Checking || upd.state == UpdateState::Downloading);
                            if (etch::button({.id = etch::id("abt_upd_chk"), .label = app.ts("update.check", "Check for Updates"),
                                              .icon_id = app.icons.refresh, .variant = etch::ButtonVariant::Default,
                                              .height = etch::Sizing::fixed(26.0f), .disabled = busy})) {
                                app.updater.check_for_updates(OMPE_VERSION_STRING, OMPE_GITHUB_REPO);
                            }
                        }
                    }

                    if (upd.state == UpdateState::Downloading) {
                        ETCH_BOX({.width = etch::Sizing::grow(), .height = etch::Sizing::fixed(3.0f),
                                  .background = pal.border_subtle, .radius = etch::Corners::all(2.0f), .clip_children = true}) {
                            etch::box({.width = etch::Sizing::percent(std::clamp(upd.download_progress, 0.02f, 1.0f)),
                                       .height = etch::Sizing::grow(), .background = pal.accent});
                        }
                    } else if (upd.state == UpdateState::Available && !upd.release_url.empty()) {
                        if (!upd.release_notes.empty()) {
                            etch::text(etch::str_cstr(upd.release_notes.c_str()), {
                                .color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.body, .wrap = true, .ellipsis = true, .max_lines = 3
                            });
                        }
                        if (etch::button({.id = etch::id("abt_upd_notes"), .label = app.ts("update.release_notes", "View Release Notes"),
                                          .icon_id = app.icons.external, .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(22.0f)})) {
                            open_external_url(upd.release_url);
                        }
                    }
                }
            }

            // Attribution & License
            etch::text(app.ts("about.attribution",
                "Built in C++20 on Etch, Inlay, SDL3 GPU, HarfBuzz, and Slug.\nSlug shader code Copyright 2017 by Eric Lengyel. MIT Licensed."), {
                .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.body, .wrap = true, .line_height_px = ts.meta * 1.55f
            });

            // Actions
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f}) {
                const bool diag_copied = (app.copied_key == "about_diag");
                if (etch::button({.id = etch::id("about_copy_diag"),
                                  .label = diag_copied ? app.ts("about.copied_diag", "Copied Diagnostics") : app.ts("about.copy_diag", "Copy Diagnostics"),
                                  .icon_id = diag_copied ? app.icons.check : app.icons.copy,
                                  .variant = etch::ButtonVariant::Primary,
                                  .width = etch::Sizing::grow(), .height = etch::Sizing::fixed(32.0f)})) {
                    const std::string diag =
                        "OMP Desktop v" OMPE_VERSION_STRING "\n"
                        "OMP CLI: " + app.omp_version_label + "\n"
                        "Etch: " ETCH_VERSION_STRING " (" + app.gpu_driver_label + ")\n"
                        "Inlay: " INLAY_VERSION_STRING "\n"
                        "OS: " + pi.os_name + " " + pi.arch + "\n"
                        "Agent Dir: " + app.config.agent_dir();
                    app.copy_with_feedback("about_diag", diag, etch::pointer_pos());
                }
                if (etch::button({.id = etch::id("about_open_omp_dir"), .label = app.ts("about.open_omp_dir", "Open ~/.omp"),
                                  .icon_id = app.icons.folder_open, .variant = etch::ButtonVariant::Default,
                                  .height = etch::Sizing::fixed(32.0f)})) {
                    reveal_in_file_manager(app.config.agent_dir());
                }
            }
        }
    }

    if (etch::clicked(scrim_id) && !etch::hovered(card_id)) {
        app.about_open = false;
    }
}

} // namespace ompe
