#include "ui/app.h"
#include "ui/thinking_ui.h"
#include "core/platform.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace ompe {
namespace {

constexpr uint32_t kDialogAttachFile = 102u;

void build_queue_chips(App& app, const Palette& pal) {
    const auto& st = app.client.state();
    if (st.steering_queue.empty() && st.follow_up_queue.empty()) return;
    const TypeScale ts = app.type();

    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = {.bottom = 6.0f},
        .gap = 4.0f,
    }) {
        auto render_chip = [&](const std::string& text, bool is_steer, uint32_t idx) {
            const uint32_t chip_id = etch::id_index(etch::id(is_steer ? "q_steer" : "q_fup"), idx);
            const uint32_t pro_id  = etch::id_index(etch::id("q_promote"), idx);
            const uint32_t rm_id   = etch::id_index(etch::id(is_steer ? "q_rm_s" : "q_rm_f"), idx);

            ETCH_BOX({
                .id = chip_id,
                .axis = etch::Axis::Row,
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::fixed(ts.pill_h + 2.0f),
                .padding = etch::Padding::xy(10.0f, 0.0f),
                .gap = 8.0f,
                .align_y = etch::AlignY::Center,
                .background = pal.bg_surface,
                .border_color = is_steer ? pal.accent_border : pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusSm),
                .shrink_width = true,
                .continuous_corners = true,
            }) {
                etch::icon({
                    .icon_id = is_steer ? app.icons.steer : app.icons.clock,
                    .size_px = 12.5f,
                    .color = is_steer ? pal.accent : pal.secondary,
                });
                etch::text(is_steer ? app.ts("composer.queue_steer", "STEER") : app.ts("composer.queue_next", "NEXT"), {
                    .color = is_steer ? pal.accent : pal.secondary,
                    .font_size_px = ts.micro,
                    .font_id = app.fonts.mono_tnum,
                    .letter_spacing = app.is_cjk_locale() ? 0.2f : 0.6f,
                    .wrap = false,
                });
                ETCH_BOX({.width = etch::Sizing::grow(), .clip_children = true, .shrink_width = true}) {
                    etch::text(etch::str_cstr(text.c_str()), {
                        .color = pal.text_secondary,
                        .font_size_px = ts.ui_sm,
                        .font_id = app.fonts.body,
                        .wrap = false,
                        .ellipsis = true,
                    });
                }

                if (!is_steer) {
                    const bool ph = etch::hovered(pro_id);
                    ETCH_BOX({
                        .id = pro_id,
                        .padding = etch::Padding::xy(5.0f, 2.0f),
                        .background = ph ? pal.accent_soft : etch::Color::transparent(),
                        .radius = etch::Corners::all(4.0f),
                    }) {
                        etch::icon({.icon_id = app.icons.arrow_up, .size_px = 11.0f, .color = ph ? pal.accent : pal.text_muted});
                    }
                    if (ph) etch::set_cursor(etch::Cursor::Hand);
                    etch::set_tooltip(pro_id, app.ts("composer.promote_steer", "Promote to immediate steer"));
                    if (etch::clicked(pro_id)) {
                        const etch::Rect r = etch::element_bounds(pro_id);
                        app.particles.spawn_point_spark({r.x + r.w * 0.5f, r.y + r.h * 0.5f}, pal.accent, 14);
                        if (!app.showcase_mode) app.client.promote_queued(text);
                    }
                }

                const bool rh = etch::hovered(rm_id);
                ETCH_BOX({
                    .id = rm_id,
                    .padding = etch::Padding::xy(4.0f, 2.0f),
                    .background = rh ? pal.danger_soft : etch::Color::transparent(),
                    .radius = etch::Corners::all(4.0f),
                }) {
                    etch::icon({.icon_id = app.icons.close, .size_px = 10.5f, .color = rh ? pal.danger : pal.text_muted});
                }
                if (rh) etch::set_cursor(etch::Cursor::Hand);
                if (etch::clicked(rm_id)) {
                    app.particles.spawn_dissolve_rect(etch::element_bounds(chip_id), pal.bg_elevated,
                                                      is_steer ? pal.accent : pal.secondary, DissolveStyle::Dust, 60);
                    if (app.showcase_mode) {
                        auto& q = is_steer ? app.client.state().steering_queue : app.client.state().follow_up_queue;
                        if (idx < q.size()) q.erase(q.begin() + idx);
                    } else {
                        app.client.remove_queued(text, is_steer ? "steering" : "followUp");
                    }
                }
            }
        };

        for (uint32_t i = 0; i < st.steering_queue.size(); ++i) render_chip(st.steering_queue[i], true, i);
        for (uint32_t i = 0; i < st.follow_up_queue.size(); ++i) render_chip(st.follow_up_queue[i], false, i);
    }
}

void build_extension_ui_card(App& app, const Palette& pal) {
    if (app.client.pending_ui_requests().empty()) return;
    const ExtensionUiRequest& req = app.client.pending_ui_requests().front();

    if (app.ask_draft.request_id != req.id) {
        app.ask_draft.request_id = req.id;
        app.ask_draft.select_index = req.initial_index;
        app.ask_draft.single_input[0] = '\0';
        if (!req.prefill.empty()) {
            const size_t n = std::min(sizeof(app.ask_draft.single_input) - 1, req.prefill.size());
            std::memcpy(app.ask_draft.single_input, req.prefill.data(), n);
            app.ask_draft.single_input[n] = '\0';
        }
        app.ask_draft.selected_indices.assign(req.questions.size(), {});
        app.ask_draft.custom_inputs.assign(req.questions.size(), {});
        for (size_t qi = 0; qi < req.questions.size(); ++qi) {
            if (req.questions[qi].recommended >= 0 &&
                static_cast<size_t>(req.questions[qi].recommended) < req.questions[qi].options.size()) {
                app.ask_draft.selected_indices[qi].push_back(req.questions[qi].recommended);
            }
        }
    }

    const TypeScale ts = app.type();
    const uint32_t card_id = etch::id("ext_ui_card");
    ETCH_BOX({
        .id = card_id,
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::all(14.0f),
        .gap = 10.0f,
        .background = pal.bg_elevated,
        .border_color = pal.accent_border,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .shadow_color = pal.shadow,
        .shadow_blur = 22.0f,
        .shadow_offset = {0.0f, 8.0f},
        .continuous_corners = true,
        .top_rim_alpha = pal.dark ? 0.20f : 0.0f,
        .margin = {.bottom = 8.0f},
    }) {
        ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
            etch::icon({.icon_id = app.icons.shield_check, .size_px = 15.0f, .color = pal.accent});
            etch::text(etch::str_cstr(req.title.empty() ? app.t("composer.action_required", "Action Required") : req.title.c_str()), {
                .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.semibold, .wrap = true
            });
            etch::box({.width = etch::Sizing::grow()});
            if (etch::button({.id = etch::id("ext_ui_cancel"), .label = app.ts("common.dismiss", "Dismiss"),
                              .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(24.0f)})) {
                app.particles.spawn_dissolve_rect(etch::element_bounds(card_id), pal.bg_elevated, pal.accent, DissolveStyle::Dust, 68);
                app.client.reply_ui_cancel(req.id);
            }
        }

        if (!req.message.empty()) {
            etch::text(etch::str_cstr(req.message.c_str()), {
                .color = pal.text_secondary, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true
            });
        }

        if (req.method == "confirm") {
            ETCH_BOX({.axis = etch::Axis::Row, .gap = 8.0f}) {
                if (etch::button({.id = etch::id("ext_confirm_yes"), .label = app.ts("common.allow", "Allow"),
                                  .icon_id = app.icons.check,
                                  .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(30.0f)})) {
                    app.particles.spawn_dissolve_rect(etch::element_bounds(card_id), pal.bg_elevated, pal.success, DissolveStyle::Dust, 64);
                    app.client.reply_ui_confirm(req.id, true);
                }
                if (etch::button({.id = etch::id("ext_confirm_no"), .label = app.ts("common.deny", "Deny"),
                                  .icon_id = app.icons.close,
                                  .variant = etch::ButtonVariant::Default, .height = etch::Sizing::fixed(30.0f)})) {
                    app.particles.spawn_dissolve_rect(etch::element_bounds(card_id), pal.bg_elevated, pal.danger, DissolveStyle::Ember, 56);
                    app.client.reply_ui_confirm(req.id, false);
                }
            }
        } else if (req.method == "select") {
            ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 4.0f}) {
                for (size_t i = 0; i < req.options.size(); ++i) {
                    const uint32_t oid = etch::id_index(etch::id("ext_sel_opt"), static_cast<uint32_t>(i));
                    const bool hov = etch::hovered(oid);
                    ETCH_BOX({
                        .id = oid,
                        .axis = etch::Axis::Row,
                        .width = etch::Sizing::grow(),
                        .padding = etch::Padding::xy(10.0f, 8.0f),
                        .gap = 10.0f,
                        .align_y = etch::AlignY::Center,
                        .background = hov ? pal.hover : pal.bg_surface,
                        .border_color = hov ? pal.accent_border : pal.border_subtle,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(kRadiusSm),
                        .continuous_corners = true,
                    }) {
                        char num[8];
                        std::snprintf(num, sizeof(num), "%zu", i + 1);
                        ETCH_BOX({
                            .width = etch::Sizing::fixed(20.0f),
                            .height = etch::Sizing::fixed(20.0f),
                            .align_x = etch::AlignX::Center,
                            .align_y = etch::AlignY::Center,
                            .background = hov ? pal.accent_soft : pal.bg_canvas,
                            .border_color = hov ? pal.accent_border : pal.border_subtle,
                            .border_width = 1.0f,
                            .radius = etch::Corners::all(5.0f),
                        }) {
                            etch::text(etch::str_cstr(num), {
                                .color = hov ? pal.accent : pal.text_faint,
                                .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                            });
                        }
                        ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 2.0f}) {
                            etch::text(etch::str_cstr(req.options[i].c_str()), {
                                .color = pal.text_primary, .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = true
                            });
                            if (i < req.option_descriptions.size() && !req.option_descriptions[i].empty()) {
                                etch::text(etch::str_cstr(req.option_descriptions[i].c_str()), {
                                    .color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.body, .wrap = true
                                });
                            }
                        }
                    }
                    if (hov) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(oid)) {
                        app.particles.spawn_dissolve_rect(etch::element_bounds(card_id), pal.bg_elevated, pal.accent, DissolveStyle::Dust, 60);
                        app.client.reply_ui_value(req.id, req.options[i]);
                    }
                }
            }
        } else if (req.method == "input" || req.method == "editor") {
            auto ir = etch::text_input({
                .id = etch::id("ext_ui_input"),
                .buffer = app.ask_draft.single_input,
                .capacity = sizeof(app.ask_draft.single_input),
                .placeholder = etch::str_cstr(req.placeholder.empty() ? app.t("composer.response_placeholder", "Type your response...") : req.placeholder.c_str()),
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::fixed(req.method == "editor" ? 72.0f : 34.0f),
                .font_size_px = ts.ui,
                .multiline = (req.method == "editor"),
                .word_wrap = true,
                .password = req.secret,
                .submit_on_enter = (req.method == "input"),
                .focus_glow_blur = 0.0f,
            });
            if (ir.submitted || etch::button({.id = etch::id("ext_ui_submit_in"), .label = app.ts("common.submit", "Submit"),
                                              .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(28.0f)})) {
                app.particles.spawn_dissolve_rect(etch::element_bounds(card_id), pal.bg_elevated, pal.accent, DissolveStyle::Dust, 60);
                app.client.reply_ui_value(req.id, app.ask_draft.single_input);
            }
        } else if (req.method == "ask") {
            for (size_t qi = 0; qi < req.questions.size(); ++qi) {
                const auto& q = req.questions[qi];
                ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 6.0f}) {
                    etch::text(etch::str_cstr(q.question.c_str()), {
                        .color = pal.text_primary, .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = true
                    });
                    ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 6.0f, .wrap = true}) {
                        for (size_t oi = 0; oi < q.options.size(); ++oi) {
                            const uint32_t bid = etch::id_index(etch::id_index(etch::id("ask_opt"), static_cast<uint32_t>(qi)), static_cast<uint32_t>(oi));
                            auto& sel = app.ask_draft.selected_indices[qi];
                            const bool chosen = std::find(sel.begin(), sel.end(), static_cast<int>(oi)) != sel.end();
                            const bool hov = etch::hovered(bid);
                            ETCH_BOX({
                                .id = bid,
                                .axis = etch::Axis::Row,
                                .padding = etch::Padding::xy(10.0f, 5.0f),
                                .gap = 5.0f,
                                .align_y = etch::AlignY::Center,
                                .background = chosen ? pal.accent_soft : (hov ? pal.hover : pal.bg_surface),
                                .border_color = chosen ? pal.accent : pal.border_subtle,
                                .border_width = 1.0f,
                                .radius = etch::Corners::all(kRadiusPill),
                                .continuous_corners = true,
                            }) {
                                if (chosen) {
                                    etch::icon({.icon_id = app.icons.check, .size_px = 11.0f, .color = pal.accent});
                                }
                                etch::text(etch::str_cstr(q.options[oi].label.c_str()), {
                                    .color = chosen ? pal.accent : pal.text_secondary,
                                    .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = false
                                });
                            }
                            if (hov) etch::set_cursor(etch::Cursor::Hand);
                            if (etch::clicked(bid)) {
                                if (q.multi) {
                                    if (chosen) sel.erase(std::remove(sel.begin(), sel.end(), static_cast<int>(oi)), sel.end());
                                    else sel.push_back(static_cast<int>(oi));
                                } else {
                                    sel.clear();
                                    sel.push_back(static_cast<int>(oi));
                                    app.ask_draft.custom_inputs[qi][0] = '\0';
                                }
                            }
                        }
                    }
                }
            }
            if (etch::button({.id = etch::id("ext_ask_send"), .label = app.ts("composer.send_answers", "Send Answers"),
                              .icon_id = app.icons.check,
                              .variant = etch::ButtonVariant::Primary, .height = etch::Sizing::fixed(30.0f)})) {
                std::vector<std::tuple<std::string, std::vector<std::string>, std::string>> answers;
                for (size_t qi = 0; qi < req.questions.size(); ++qi) {
                    std::vector<std::string> labels;
                    for (int idx : app.ask_draft.selected_indices[qi]) {
                        if (idx >= 0 && static_cast<size_t>(idx) < req.questions[qi].options.size()) {
                            labels.push_back(req.questions[qi].options[idx].label);
                        }
                    }
                    answers.emplace_back(req.questions[qi].id, std::move(labels), std::string(app.ask_draft.custom_inputs[qi].data()));
                }
                app.particles.spawn_dissolve_rect(etch::element_bounds(card_id), pal.bg_elevated, pal.accent, DissolveStyle::Dust, 72);
                app.client.reply_ui_ask(req.id, answers);
            }
        }
    }
}

} // namespace

void build_composer(App& app) {
    const Palette& pal = app.palette();
    const TypeScale ts = app.type();
    const bool streaming = app.client.state().is_streaming;
    const uint32_t composer_wrap_id = etch::id("composer_wrap");
    const etch::Rect wrap_b = etch::element_bounds(composer_wrap_id);
    const float card_w = (wrap_b.w > 120.0f) ? std::min(kReadingMaxW, wrap_b.w - 48.0f) : 760.0f;

    ETCH_BOX({
        .id = composer_wrap_id,
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = {.left = 24.0f, .right = 24.0f, .top = 4.0f, .bottom = 16.0f},
        .align_x = etch::AlignX::Center,
    }) {
        ETCH_BOX({
            .axis = etch::Axis::Col,
            .width = etch::Sizing::fixed(card_w),
            .gap = 4.0f,
        }) {
            build_extension_ui_card(app, pal);

            if (app.client.live_voice_active() && app.ambient_shader) {
                ETCH_BOX({
                    .id = etch::id("voice_ribbon"),
                    .axis = etch::Axis::Row,
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::fixed(44.0f),
                    .padding = etch::Padding::xy(14.0f, 0.0f),
                    .align_y = etch::AlignY::Center,
                    .background = pal.bg_surface,
                    .border_color = pal.accent_border,
                    .border_width = 1.0f,
                    .radius = etch::Corners::all(kRadiusMd),
                    .clip_children = true,
                    .continuous_corners = true,
                }) {
                    etch::ShaderSurfaceEvalDesc ev{};
                    ev.width = static_cast<uint32_t>(std::max(240.0f, card_w));
                    ev.height = 44u;
                    ev.params[0] = {pal.shader_base[0], pal.shader_base[1], pal.shader_base[2], pal.shader_base[3]};
                    ev.params[1] = {pal.shader_accent[0], pal.shader_accent[1], pal.shader_accent[2], 0.8f};
                    ev.params[2] = {pal.shader_secondary[0], pal.shader_secondary[1], pal.shader_secondary[2],
                                    std::max(app.client.live_input_level(), app.client.live_output_level())};
                    ev.params[3] = {0.5f, 0.5f, 1.0f, 1.0f};
                    if (auto* tex = etch::eval_shader_surface(app.ctx, app.ambient_shader, ev)) {
                        etch::draw_image({.texture = tex, .rect = {0.0f, 0.0f, card_w, 44.0f}});
                    }
                    const std::string vp = app.tf("composer.voice_phase", "LIVE · {phase}", {
                        {"phase", app.client.live_voice_phase().empty() ? app.t("composer.voice_listening", "listening") : app.client.live_voice_phase()}
                    });
                    etch::text(etch::str_cstr(vp.c_str()), {
                        .color = pal.accent, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                    });
                    etch::box({.width = etch::Sizing::grow()});
                    if (etch::button({.id = etch::id("voice_stop_btn"), .label = app.ts("composer.end_voice", "End Voice"),
                                      .variant = etch::ButtonVariant::Danger, .height = etch::Sizing::fixed(24.0f)})) {
                        app.client.stop_live_voice();
                    }
                }
            }

            build_queue_chips(app, pal);

            if (!app.pending_images.empty() || !app.attached_files.empty()) {
                ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .padding = {.bottom = 4.0f}, .gap = 6.0f, .wrap = true}) {
                    for (size_t i = 0; i < app.pending_images.size(); ++i) {
                        const uint32_t pid = etch::id_index(etch::id("att_img"), static_cast<uint32_t>(i));
                        ETCH_BOX({
                            .id = pid,
                            .axis = etch::Axis::Row,
                            .height = etch::Sizing::fixed(26.0f),
                            .padding = etch::Padding::xy(8.0f, 0.0f),
                            .gap = 5.0f,
                            .align_y = etch::AlignY::Center,
                            .background = pal.accent_soft,
                            .border_color = pal.accent_border,
                            .border_width = 1.0f,
                            .radius = etch::Corners::all(kRadiusPill),
                        }) {
                            etch::icon({.icon_id = app.icons.image_frame, .size_px = 12.0f, .color = pal.accent});
                            etch::text(etch::str_cstr(app.pending_images[i].filename.c_str()), {
                                .color = pal.text_primary, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                            });
                            etch::icon({.icon_id = app.icons.close, .size_px = 9.5f, .color = pal.text_muted});
                        }
                        if (etch::clicked(pid)) {
                            app.particles.spawn_dissolve_rect(etch::element_bounds(pid), pal.bg_elevated, pal.accent, DissolveStyle::Dust, 36);
                            app.pending_images.erase(app.pending_images.begin() + i);
                            break;
                        }
                    }
                    for (size_t i = 0; i < app.attached_files.size(); ++i) {
                        const uint32_t fid = etch::id_index(etch::id("att_file"), static_cast<uint32_t>(i));
                        const std::string fn = fs::path(app.attached_files[i]).filename().string();
                        ETCH_BOX({
                            .id = fid,
                            .axis = etch::Axis::Row,
                            .height = etch::Sizing::fixed(26.0f),
                            .padding = etch::Padding::xy(8.0f, 0.0f),
                            .gap = 5.0f,
                            .align_y = etch::AlignY::Center,
                            .background = pal.bg_surface,
                            .border_color = pal.border_default,
                            .border_width = 1.0f,
                            .radius = etch::Corners::all(kRadiusPill),
                        }) {
                            etch::icon({.icon_id = app.icons.file_code, .size_px = 12.0f, .color = pal.secondary});
                            etch::text(etch::str_cstr(fn.c_str()), {
                                .color = pal.text_secondary, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                            });
                            etch::icon({.icon_id = app.icons.close, .size_px = 9.5f, .color = pal.text_muted});
                        }
                        if (etch::clicked(fid)) {
                            app.particles.spawn_dissolve_rect(etch::element_bounds(fid), pal.bg_elevated, pal.secondary, DissolveStyle::Dust, 36);
                            app.attached_files.erase(app.attached_files.begin() + i);
                            break;
                        }
                    }
                }
            }

            // Sculpted Composer Capsule — BorderBeam activates ONLY while streaming
            const bool drag_over = etch::os_drag_hovering(etch::id("composer_card"));
            const bool focused = etch::focused(etch::id("composer_input"));
            const etch::BorderBeamStyle beam = streaming ? etch::BorderBeamStyle{
                .color = pal.accent,
                .base_color = pal.border_default,
                .width = 1.5f,
                .sweep_fraction = 0.24f,
                .glow_px = 6.0f,
                .speed = 1.5f,
                .beam_active = true,
            } : etch::BorderBeamStyle{};

            ETCH_BOX({
                .id = etch::id("composer_card"),
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .padding = {.left = 14.0f, .right = 12.0f, .top = 10.0f, .bottom = 8.0f},
                .gap = 6.0f,
                .background = pal.bg_surface,
                .border_color = drag_over ? pal.accent : (focused ? pal.border_strong : pal.border_default),
                .border_width = streaming ? 0.0f : 1.0f,
                .radius = etch::Corners::all(kRadiusLg),
                .shadow_color = pal.shadow,
                .shadow_blur = 22.0f,
                .shadow_offset = {0.0f, 8.0f},
                .continuous_corners = true,
                .shadow_spread = -6.0f,
                .border_beam = beam,
                .top_rim_alpha = pal.dark ? 0.14f : 0.0f,
            }) {
                const auto ir = etch::text_input({
                    .id = etch::id("composer_input"),
                    .buffer = app.composer_buf,
                    .capacity = sizeof(app.composer_buf),
                    .placeholder = etch::str_cstr(streaming
                        ? app.t("composer.placeholder_streaming", "Steer or queue follow-up...")
                        : app.t("composer.placeholder_idle", "Ask OMP anything, /command, or drop files...")),
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::fixed(50.0f),
                    .padding = etch::Padding::xy(2.0f, 2.0f),
                    .font_size_px = app.config.prefs().font_size_px,
                    .font_id = app.fonts.body,
                    .text_color = pal.text_primary,
                    .placeholder_color = pal.text_faint,
                    .selection_color = pal.accent_soft,
                    .caret_color = pal.accent,
                    .background = etch::Color::transparent(),
                    .border_color = etch::Color::transparent(),
                    .focus_border_color = etch::Color::transparent(),
                    .border_width = 0.0f,
                    .focus_border_width = 0.0f,
                    .multiline = true,
                    .word_wrap = true,
                    .ghost_text = etch::str_cstr(app.client.ghost_prediction().c_str()),
                    .ghost_color = pal.text_faint,
                    .accept_ghost_on_tab = true,
                    .submit_on_enter = true,
                    .focus_glow_blur = 0.0f,
                });

                if (ir.ghost_accepted && !app.showcase_mode) {
                    app.client.send_word_prediction_feedback(app.last_predicted_draft,
                                                             utf8_to_utf16_offset(app.last_predicted_draft, app.last_predicted_draft.size()),
                                                             app.client.ghost_prediction(), true);
                    app.client.clear_ghost_prediction();
                }

                if (ir.changed && !app.showcase_mode && app.client.status() == ConnectionStatus::Connected) {
                    const int64_t now = unix_time_ms();
                    const std::string cur(app.composer_buf);
                    if (cur.size() >= 3 && cur[0] != '/' && (now - app.last_predict_sent_ms) >= 180) {
                        app.last_predict_sent_ms = now;
                        app.last_predicted_draft = cur;
                        app.client.request_word_prediction(cur, utf8_to_utf16_offset(cur, ir.cursor_byte));
                    } else if (cur.empty()) {
                        app.client.clear_ghost_prediction();
                    }
                }

                if (ir.submitted) {
                    const bool steer = (etch::key_mods() & etch::Mod_Alt) != 0;
                    app.trigger_send(steer);
                }

                // Minimal bottom control strip with state-reactive icons
                ETCH_BOX({
                    .axis = etch::Axis::Row,
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::fixed(ts.pill_h + 2.0f),
                    .gap = 7.0f,
                    .align_y = etch::AlignY::Center,
                }) {
                    // Model selector pill with spring-rotated chevron
                    const uint32_t mdl_id = etch::id("cmp_model_pill");
                    const bool mdl_hov = etch::hovered(mdl_id);
                    const std::string mdl_name = app.client.state().model.name.empty()
                        ? app.t("composer.select_model", "Select Model") : app.client.state().model.name;
                    const float mdl_chev_rad = etch::spring_angle(etch::id("cmp_mdl_chev"),
                                                                  app.model_popover_open ? -1.5707963f : 1.5707963f,
                                                                  kMotionSnap);
                    ETCH_BOX({
                        .id = mdl_id,
                        .axis = etch::Axis::Row,
                        .height = etch::Sizing::fixed(ts.pill_h),
                        .padding = etch::Padding::xy(10.0f, 0.0f),
                        .gap = 6.0f,
                        .align_y = etch::AlignY::Center,
                        .background = (mdl_hov || app.model_popover_open) ? pal.hover : etch::Color::transparent(),
                        .border_color = app.model_popover_open ? pal.border_strong : pal.border_subtle,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(kRadiusPill),
                        .continuous_corners = true,
                    }) {
                        etch::text(etch::str_cstr(mdl_name.c_str()), {
                            .color = pal.text_secondary, .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = false
                        });
                        etch::icon({
                            .icon_id = app.icons.chevron_r,
                            .size_px = 10.5f,
                            .color = pal.text_muted,
                            .rotation_rad = mdl_chev_rad,
                        });
                    }
                    if (mdl_hov) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(mdl_id)) {
                        app.model_popover_open = !app.model_popover_open;
                        app.thinking_popover_open = false;
                        app.workspace_popover_open = false;
                        app.model_search[0] = '\0';
                    }

                    // Thinking Effort tactile 4-bar meter pill -> opens Thinking Effort Popover (Alt+Click quick-cycles)
                    const uint32_t thk_id = etch::id("cmp_think_pill");
                    const bool thk_hov = etch::hovered(thk_id);
                    const auto& avail_lvls = app.client.available_thinking_levels();
                    const bool thk_supported = (avail_lvls.size() > 1) ||
                                               (!avail_lvls.empty() && avail_lvls[0] != "off");
                    const std::string& thk = app.client.state().thinking_level;
                    const ThinkingLevelMeta thk_meta = thinking_level_meta(thk_supported ? std::string_view(thk) : std::string_view("off"), app.i18n);
                    const bool thk_on = thk_supported && (thk_meta.bars > 0);
                    const float thk_chev_rad = etch::spring_angle(etch::id("cmp_thk_chev"),
                                                                  app.thinking_popover_open ? -1.5707963f : 1.5707963f,
                                                                  kMotionSnap);
                    const etch::Color thk_bg = app.thinking_popover_open
                        ? pal.hover
                        : (thk_on
                            ? (thk_meta.cool_tint ? pal.secondary_soft : pal.selected_subtle)
                            : (thk_hov ? pal.hover : etch::Color::transparent()));
                    const etch::Color thk_border = app.thinking_popover_open
                        ? pal.border_strong
                        : (thk_on
                            ? (thk_meta.cool_tint
                                ? etch::Color::rgba8(pal.secondary.r, pal.secondary.g, pal.secondary.b, 92)
                                : pal.accent_border)
                            : pal.border_subtle);
                    const etch::Color thk_fg = !thk_supported
                        ? pal.text_faint
                        : (thk_on
                            ? (thk_meta.cool_tint ? pal.secondary : pal.accent)
                            : (thk_hov ? pal.text_secondary : pal.text_muted));

                    ETCH_BOX({
                        .id = thk_id,
                        .axis = etch::Axis::Row,
                        .height = etch::Sizing::fixed(ts.pill_h),
                        .padding = etch::Padding::xy(10.0f, 0.0f),
                        .gap = 6.0f,
                        .align_y = etch::AlignY::Center,
                        .background = thk_bg,
                        .border_color = thk_border,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(kRadiusPill),
                        .continuous_corners = true,
                    }) {
                        render_thinking_bars(thk_meta, pal, !thk_supported);
                        etch::text(etch::str_cstr(thk_meta.short_label), {
                            .color = thk_fg,
                            .font_size_px = ts.ui_sm,
                            .font_id = app.fonts.medium,
                            .wrap = false,
                        });
                        etch::icon({
                            .icon_id = app.icons.chevron_r,
                            .size_px = 10.0f,
                            .color = thk_on ? thk_fg : pal.text_faint,
                            .rotation_rad = thk_chev_rad,
                        });
                    }
                    if (thk_hov) etch::set_cursor(etch::Cursor::Hand);
                    etch::set_tooltip(thk_id, thk_supported
                        ? app.ts("composer.thinking_tooltip", "Thinking effort · Click to choose · Alt+Click or Ctrl+Shift+T to cycle")
                        : app.ts("composer.thinking_unsupported_tooltip", "Current model does not support thinking effort"));
                    if (etch::clicked(thk_id)) {
                        if ((etch::key_mods() & etch::Mod_Alt) != 0) {
                            app.thinking_popover_open = false;
                            app.cycle_thinking_effort();
                        } else {
                            app.thinking_popover_open = !app.thinking_popover_open;
                            app.model_popover_open = false;
                            app.workspace_popover_open = false;
                        }
                    }

                    // Attach file/image button
                    const uint32_t att_id = etch::id("cmp_attach");
                    const bool att_hov = etch::hovered(att_id);
                    ETCH_BOX({
                        .id = att_id,
                        .width = etch::Sizing::fixed(ts.pill_h),
                        .height = etch::Sizing::fixed(ts.pill_h),
                        .align_x = etch::AlignX::Center,
                        .align_y = etch::AlignY::Center,
                        .background = att_hov ? pal.hover : etch::Color::transparent(),
                        .radius = etch::Corners::all(kRadiusPill),
                    }) {
                        etch::icon({.icon_id = app.icons.paperclip, .size_px = 14.5f, .color = att_hov ? pal.text_primary : pal.text_muted});
                    }
                    if (att_hov) etch::set_cursor(etch::Cursor::Hand);
                    etch::set_tooltip(att_id, app.ts("composer.attach_tooltip", "Attach image or file"));
                    if (etch::clicked(att_id) && !app.headless_mode) {
                        etch::open_file_dialog(app.ctx, app.window, {}, app.config.prefs().active_workspace.c_str(), kDialogAttachFile, true);
                    }

                    etch::box({.width = etch::Sizing::grow()});

                    // Compact 16px Context Ring Gauge
                    if (app.client.state().context_usage.valid) {
                        const uint32_t ring_id = etch::id("cmp_ctx_ring");
                        const bool r_hov = etch::hovered(ring_id);
                        const float pct = std::clamp(static_cast<float>(app.client.state().context_usage.percent) / 100.0f, 0.0f, 1.0f);
                        const float anim_pct = etch::spring(etch::id("ctx_ring_spring"), pct, pct, kMotionGentle);
                        const etch::Color ring_col = (pct > 0.85f) ? pal.danger : ((pct > 0.65f) ? pal.warning : pal.secondary);

                        ETCH_BOX({
                            .id = ring_id,
                            .width = etch::Sizing::fixed(26.0f),
                            .height = etch::Sizing::fixed(26.0f),
                            .align_x = etch::AlignX::Center,
                            .align_y = etch::AlignY::Center,
                            .background = r_hov ? pal.hover : etch::Color::transparent(),
                            .radius = etch::Corners::all(kRadiusPill),
                        }) {
                            etch::draw_ring({
                                .center = {13.0f, 13.0f},
                                .radius = 7.0f,
                                .thickness = 2.0f,
                                .start_angle_rad = -1.5707963f,
                                .sweep_angle_rad = std::max(0.15f, anim_pct * 6.2831853f),
                                .color = ring_col,
                                .track_color = pal.border_default,
                            });
                        }
                        if (r_hov) etch::set_cursor(etch::Cursor::Hand);
                        char pct_buf[16];
                        std::snprintf(pct_buf, sizeof(pct_buf), "%.0f", app.client.state().context_usage.percent);
                        const std::string tip = app.tf("composer.context_tooltip",
                            "Context {pct}% ({used} / {total}) · {cost} · Click to compact", {
                                {"pct", pct_buf},
                                {"used", format_compact_tokens(app.client.state().context_usage.tokens)},
                                {"total", format_compact_tokens(app.client.state().context_usage.context_window)},
                                {"cost", format_usd(app.client.stats().cost_usd)},
                            });
                        etch::set_tooltip(ring_id, etch::str_cstr(tip.c_str()));
                        if (etch::clicked(ring_id) && !app.showcase_mode) {
                            app.client.compact_now();
                        }
                    }

                    // State-reactive Live Voice toggle (mic -> pulsing audio_lines when active)
                    const uint32_t mic_id = etch::id("cmp_mic");
                    const bool mic_hov = etch::hovered(mic_id);
                    const bool v_on = app.client.live_voice_active();
                    const float voice_pulse = v_on ? (1.0f + 0.12f * std::sin(app.elapsed_s * 6.5f)) : 1.0f;
                    ETCH_BOX({
                        .id = mic_id,
                        .width = etch::Sizing::fixed(26.0f),
                        .height = etch::Sizing::fixed(26.0f),
                        .align_x = etch::AlignX::Center,
                        .align_y = etch::AlignY::Center,
                        .background = v_on ? pal.accent_soft : (mic_hov ? pal.hover : etch::Color::transparent()),
                        .radius = etch::Corners::all(kRadiusPill),
                    }) {
                        etch::icon({
                            .icon_id = v_on ? app.icons.audio_lines : app.icons.mic,
                            .size_px = 13.5f,
                            .color = v_on ? pal.accent : (mic_hov ? pal.text_primary : pal.text_muted),
                            .scale = voice_pulse,
                        });
                    }
                    if (mic_hov) etch::set_cursor(etch::Cursor::Hand);
                    etch::set_tooltip(mic_id, app.ts("composer.voice_tooltip", "Toggle Live Voice (/live)"));
                    if (etch::clicked(mic_id) && !app.showcase_mode) {
                        if (v_on) app.client.stop_live_voice();
                        else app.client.start_live_voice();
                    }

                    // State-reactive Action button: Send (arrow_up) / Steer (corner_down_left) / Stop (duotone stop)
                    const bool has_draft = (app.composer_buf[0] != '\0') || !app.pending_images.empty() || !app.attached_files.empty();
                    const bool show_stop = streaming && !has_draft;
                    const bool show_steer = streaming && has_draft;
                    const uint32_t act_id = etch::id("cmp_action_btn");
                    const bool act_hov = etch::hovered(act_id);
                    const etch::Color btn_bg = show_stop
                        ? pal.danger
                        : (has_draft ? pal.accent : pal.bg_elevated);
                    const etch::Color btn_fg = show_stop
                        ? etch::Color::white()
                        : (has_draft ? pal.on_accent : pal.text_faint);
                    const uint16_t act_ico = show_stop ? app.icons.stop : (show_steer ? app.icons.steer : app.icons.send);

                    ETCH_BOX({
                        .id = act_id,
                        .width = etch::Sizing::fixed(28.0f),
                        .height = etch::Sizing::fixed(28.0f),
                        .align_x = etch::AlignX::Center,
                        .align_y = etch::AlignY::Center,
                        .background = btn_bg,
                        .radius = etch::Corners::all(kRadiusPill),
                        .hover_scale = (has_draft || show_stop) ? 1.06f : 1.0f,
                        .press_scale = 0.92f,
                        .transition = kMotionSnap,
                    }) {
                        etch::icon({.icon_id = act_ico, .size_px = 13.5f, .color = btn_fg});
                    }
                    if (act_hov && (has_draft || show_stop)) etch::set_cursor(etch::Cursor::Hand);
                    etch::set_tooltip(act_id, show_stop ? app.ts("composer.stop_tooltip", "Stop generation (Esc)")
                                                        : (show_steer ? app.ts("composer.steer_tooltip", "Queue follow-up (Alt+Enter to steer)")
                                                                      : app.ts("composer.send_tooltip", "Send message (Enter)")));
                    if (etch::clicked(act_id)) {
                        if (show_stop) {
                            app.particles.spawn_dissolve_rect(etch::element_bounds(etch::id("composer_card")),
                                                              pal.danger, pal.accent, DissolveStyle::Ember, 54);
                            if (!app.showcase_mode) app.client.abort_and_restore_queue();
                        } else if (has_draft) {
                            app.trigger_send(false);
                        }
                    }
                }
            }
        }
    }
}

} // namespace ompe
