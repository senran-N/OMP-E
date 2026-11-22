#include "ui/app.h"
#include "ui/diff_view.h"
#include "core/json.h"
#include "core/platform.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace ompe {
namespace {

bool is_exploration_tool(const std::string& name) {
    return name == "read" || name == "read_file" ||
           name == "grep" || name == "search" || name == "ripgrep" ||
           name == "find" || name == "glob" || name == "ls" || name == "list_dir" ||
           name == "web_search" || name == "fetch" || name == "lsp";
}

bool is_mutation_tool(const std::string& name) {
    return is_mutation_tool_name(name);
}

bool is_shell_tool(const std::string& name) {
    return name == "bash" || name == "shell" || name == "run_command";
}

bool is_plan_tool(const std::string& name) {
    return name == "todo_write" || name == "update_plan" || name == "plan" || name == "todo";
}

bool is_subagent_tool(const std::string& name) {
    return name == "task" || name == "subagent" || name == "dispatch_agent" || name == "invoke_subagent";
}

std::pair<std::string, std::string> split_dir_file(const std::string& path) {
    const size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return {"", path};
    return {path.substr(0, pos + 1), path.substr(pos + 1)};
}

std::string format_duration_compact(int64_t ms) {
    char buf[32];
    if (ms >= 1000) {
        std::snprintf(buf, sizeof(buf), "%.2fs", static_cast<double>(ms) / 1000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%lldms", static_cast<long long>(ms));
    }
    return buf;
}

struct PlanStep {
    std::string label;
    std::string status; // "completed" | "in_progress" | "pending"
};

std::vector<PlanStep> parse_plan_steps(const std::string& args_json, const std::string& output_text) {
    std::vector<PlanStep> steps;
    Json parsed;
    if (!args_json.empty() && Json::parse(args_json, parsed) && parsed.is_object()) {
        const Json* arr = parsed.find("todos");
        if (!arr || !arr->is_array()) arr = parsed.find("items");
        if (!arr || !arr->is_array()) arr = parsed.find("steps");
        if (arr && arr->is_array()) {
            for (const auto& item : arr->as_array()) {
                if (!item.is_object()) continue;
                PlanStep st;
                st.label = item["content"].as_string_copy();
                if (st.label.empty()) st.label = item["title"].as_string_copy();
                if (st.label.empty()) st.label = item["label"].as_string_copy();
                st.status = item["status"].as_string_copy("pending");
                if (!st.label.empty()) steps.push_back(std::move(st));
            }
        }
    }
    if (steps.empty() && !output_text.empty()) {
        std::istringstream ss(output_text);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("[x] ", 0) == 0 || line.rfind("- [x] ", 0) == 0) {
                steps.push_back({line.substr(line.find("] ") + 2), "completed"});
            } else if (line.rfind("[~] ", 0) == 0 || line.rfind("- [~] ", 0) == 0) {
                steps.push_back({line.substr(line.find("] ") + 2), "in_progress"});
            } else if (line.rfind("[ ] ", 0) == 0 || line.rfind("- [ ] ", 0) == 0) {
                steps.push_back({line.substr(line.find("] ") + 2), "pending"});
            }
        }
    }
    return steps;
}

void render_bash_terminal_block(const ToolExecution& te, const Palette& pal, const FontSlots& fonts, const TypeScale& ts) {
    // Dedicated dark instrument surface in BOTH Dark and Light themes so ANSI escapes remain razor-sharp
    const etch::Color term_bg = pal.dark ? pal.bg_code : etch::Color::rgb_hex(0x161619);
    const etch::Color term_fg = pal.dark ? pal.text_body : etch::Color::rgb_hex(0xE8E3DA);

    if (te.terminal) {
        const uint32_t vis_rows = std::clamp(te.terminal->used_rows(), 3u, 14u);
        ETCH_BOX({
            .width = etch::Sizing::grow(),
            .padding = etch::Padding::xy(12.0f, 9.0f),
            .background = term_bg,
            .clip_children = true,
        }) {
            etch::terminal_grid({
                .id = etch::id(te.tool_call_id.c_str()),
                .cells = te.terminal->cells(),
                .columns = te.terminal->columns(),
                .rows = vis_rows,
                .first_row = te.terminal->first_row(),
                .cell_width_px = ts.term_cell_w,
                .line_height_px = ts.term_line_h,
                .font_size_px = ts.code,
                .font_id = fonts.mono,
                .foreground = term_fg,
                .background = term_bg,
                .cursor_column = te.running ? te.terminal->cursor_col() : -1,
                .cursor_row = te.running ? te.terminal->cursor_row() : -1,
                .cursor_color = pal.accent,
            });
        }
    } else if (!te.output_text.empty()) {
        ETCH_BOX({
            .width = etch::Sizing::grow(),
            .padding = etch::Padding::xy(12.0f, 9.0f),
            .background = term_bg,
        }) {
            etch::text(etch::str_cstr(te.output_text.c_str()), {
                .color = term_fg, .font_size_px = ts.code, .font_id = fonts.mono, .wrap = true, .selectable = true
            });
        }
    }
}

std::string msg_uid(const ChatMessage& m, size_t mi) {
    return m.id.empty() ? ("m" + std::to_string(mi)) : (m.id + "_" + std::to_string(mi));
}

// ============================================================================
// 1. Borderless Whisper Thinking Trace (bounded scroll + bottom Collapse pill!)
// ============================================================================
void render_thinking_trace(App& app, const Palette& pal, const ChatMessage& m, size_t mi, size_t bi, const ContentBlock& b) {
    const TypeScale ts = app.type();
    const std::string key = "th_" + msg_uid(m, mi) + "_" + std::to_string(bi);
    const std::string full_key = key + "_full";
    const bool open = app.is_disclosure_open(key, !b.complete);
    const bool full_mode = app.is_disclosure_open(full_key, false);
    const bool is_long = b.text.size() > 480;
    const uint32_t hdr_id = etch::id(key.c_str());
    const uint32_t body_scroll_id = etch::id_index(hdr_id, 3);
    const uint32_t collapse_btn_id = etch::id_index(hdr_id, 4);
    const uint32_t full_btn_id = etch::id_index(hdr_id, 5);
    const bool hov = etch::hovered(hdr_id);
    const float chev_rad = etch::spring_angle(etch::id_index(hdr_id, 1), open ? 1.5707963f : 0.0f, kMotionSnap);

    std::string hdr;
    if (b.started_ms > 0 && b.ended_ms > b.started_ms) {
        char sec_buf[24];
        std::snprintf(sec_buf, sizeof(sec_buf), "%.1f", static_cast<double>(b.ended_ms - b.started_ms) / 1000.0);
        hdr = app.tf("chat.thought_for", "Thought for {sec}s", {{"sec", sec_buf}});
    } else if (!b.complete) {
        hdr = app.t("chat.thinking", "Thinking...");
    } else {
        hdr = app.t("chat.thought", "Thought");
    }

    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 4.0f}) {
        ETCH_BOX({
            .id = hdr_id,
            .axis = etch::Axis::Row,
            .width = etch::Sizing::fit(),
            .height = etch::Sizing::fixed(ts.pill_h - 2.0f),
            .padding = etch::Padding::xy(7.0f, 0.0f),
            .gap = 6.0f,
            .align_y = etch::AlignY::Center,
            .background = hov ? pal.hover : etch::Color::transparent(),
            .radius = etch::Corners::all(6.0f),
        }) {
            if (!b.complete) {
                etch::icon({
                    .icon_id = app.icons.loader_arc,
                    .size_px = 13.5f,
                    .color = pal.accent,
                    .rotation_rad = app.elapsed_s * 5.2f,
                });
            } else {
                etch::icon({
                    .icon_id = open ? app.icons.brain_active : app.icons.brain_circuit,
                    .size_px = 14.0f,
                    .color = (open || hov) ? pal.accent : pal.text_muted,
                });
            }
            etch::text(etch::str_cstr(hdr.c_str()), {
                .color = (open || hov) ? pal.text_secondary : pal.text_muted,
                .font_size_px = ts.ui_sm,
                .font_id = app.fonts.ui_tnum,
                .wrap = false,
            });
            etch::icon({
                .icon_id = app.icons.chevron_r,
                .size_px = 11.5f,
                .color = pal.text_faint,
                .rotation_rad = chev_rad,
            });
        }
        if (hov) etch::set_cursor(etch::Cursor::Hand);
        if (etch::clicked(hdr_id)) {
            app.toggle_disclosure(key, !b.complete);
        }

        ETCH_COLLAPSE({
            .id = etch::id_index(hdr_id, 2),
            .open = open,
            .spring = kMotionFluid,
            .fade_content = true,
        }) {
            // Editorial vertical hairline rail — bounded scroll height by default so long reasoning never traps the user
            ETCH_BOX({
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .padding = {.left = 12.0f, .right = 6.0f, .top = 4.0f, .bottom = 4.0f},
                .gap = 6.0f,
                .border_color = pal.border_default,
                .border_sides = {.left = 1.5f},
                .margin = {.left = 11.0f},
            }) {
                ETCH_BOX({
                    .id = body_scroll_id,
                    .axis = etch::Axis::Col,
                    .width = etch::Sizing::grow(),
                    .height = (is_long && !full_mode) ? etch::Sizing::fit(0.0f, 260.0f) : etch::Sizing::fit(),
                    .padding = {.right = (is_long && !full_mode) ? 8.0f : 2.0f},
                    .clip_children = (is_long && !full_mode),
                    .scroll_y = (is_long && !full_mode),
                    .shrink_height = (is_long && !full_mode),
                    .scroll_fade_px = (is_long && !full_mode) ? 18.0f : 0.0f,
                    .stick_to_bottom = (!b.complete && is_long && !full_mode),
                }) {
                    etch::text(etch::str_cstr(b.text.c_str()), {
                        .color = pal.text_muted,
                        .font_size_px = ts.ui,
                        .font_id = app.fonts.body,
                        .wrap = true,
                        .selectable = true,
                        .line_height_px = ts.ui * 1.54f,
                    });
                }

                // Bottom whisper action bar so collapsing from the end of a long trace takes 1 click (zero scrolling up!)
                if (is_long) {
                    const bool chov = etch::hovered(collapse_btn_id);
                    const bool fhov = etch::hovered(full_btn_id);
                    ETCH_BOX({
                        .axis = etch::Axis::Row,
                        .width = etch::Sizing::grow(),
                        .gap = 8.0f,
                        .align_y = etch::AlignY::Center,
                    }) {
                        ETCH_BOX({
                            .id = collapse_btn_id,
                            .axis = etch::Axis::Row,
                            .height = etch::Sizing::fixed(22.0f),
                            .padding = etch::Padding::xy(7.0f, 0.0f),
                            .gap = 4.0f,
                            .align_y = etch::AlignY::Center,
                            .background = chov ? pal.hover : etch::Color::transparent(),
                            .radius = etch::Corners::all(5.0f),
                        }) {
                            etch::icon({.icon_id = app.icons.arrow_up, .size_px = 10.5f, .color = chov ? pal.text_primary : pal.text_faint});
                            etch::text(app.ts("chat.collapse_thinking", "Collapse"), {
                                .color = chov ? pal.text_primary : pal.text_muted,
                                .font_size_px = ts.meta,
                                .font_id = app.fonts.medium,
                                .wrap = false,
                            });
                        }
                        if (chov) etch::set_cursor(etch::Cursor::Hand);
                        if (etch::clicked(collapse_btn_id)) {
                            app.toggle_disclosure(key, !b.complete);
                        }

                        ETCH_BOX({
                            .id = full_btn_id,
                            .axis = etch::Axis::Row,
                            .height = etch::Sizing::fixed(22.0f),
                            .padding = etch::Padding::xy(7.0f, 0.0f),
                            .gap = 4.0f,
                            .align_y = etch::AlignY::Center,
                            .background = fhov ? pal.hover : etch::Color::transparent(),
                            .radius = etch::Corners::all(5.0f),
                        }) {
                            etch::text(full_mode ? app.ts("chat.compact_view", "Compact view") : app.ts("chat.expand_full", "Expand full"), {
                                .color = fhov ? pal.accent : pal.text_faint,
                                .font_size_px = ts.meta,
                                .font_id = app.fonts.medium,
                                .wrap = false,
                            });
                        }
                        if (fhov) etch::set_cursor(etch::Cursor::Hand);
                        if (etch::clicked(full_btn_id)) {
                            app.toggle_disclosure(full_key, false);
                        }
                    }
                }
            }
        }
    }
}

// ============================================================================
// 2. Zero-Card Borderless Grouped Exploration Trace (read / grep / find / ls)
// ============================================================================
void render_exploration_group(App& app, const Palette& pal, const ChatMessage& m, size_t mi,
                              size_t start_bi, size_t end_bi) {
    const TypeScale ts = app.type();
    const std::string key = "exp_" + msg_uid(m, mi) + "_" + std::to_string(start_bi);
    bool any_running = false;
    int read_count = 0, search_count = 0, other_count = 0;
    std::vector<std::string> preview_chips;

    for (size_t i = start_bi; i < end_bi; ++i) {
        const ContentBlock& b = m.blocks[i];
        const auto it = app.client.tool_executions().find(b.tool_call_id);
        const ToolExecution* te = (it != app.client.tool_executions().end()) ? &it->second : nullptr;
        if (te ? te->running : !b.complete) any_running = true;

        if (b.tool_name == "read" || b.tool_name == "read_file") ++read_count;
        else if (b.tool_name == "grep" || b.tool_name == "search" || b.tool_name == "ripgrep" || b.tool_name == "web_search") ++search_count;
        else ++other_count;

        const std::string raw = (te && !te->args_summary.empty()) ? te->args_summary : b.tool_intent;
        if (!raw.empty() && preview_chips.size() < 3) {
            if (b.tool_name == "grep" || b.tool_name == "search" || b.tool_name == "ripgrep") {
                const size_t in_pos = raw.find(" in ");
                std::string pat = (in_pos != std::string::npos) ? raw.substr(0, in_pos) : raw;
                preview_chips.push_back("/" + pat + "/");
            } else {
                auto [dir, fn] = split_dir_file(raw);
                preview_chips.push_back(fn.empty() ? raw : fn);
            }
        }
    }

    const bool open = app.is_disclosure_open(key, false);
    const uint32_t hdr_id = etch::id(key.c_str());
    const bool hov = etch::hovered(hdr_id);
    const float chev_rad = etch::spring_angle(etch::id_index(hdr_id, 1), open ? 1.5707963f : 0.0f, kMotionSnap);

    // Build localized human-readable summary ("Explored 3 files, 1 search") using extensible i18n keys
    std::string summary;
    const size_t total = end_bi - start_bi;
    if (total == 1) {
        const ContentBlock& b = m.blocks[start_bi];
        const auto it = app.client.tool_executions().find(b.tool_call_id);
        const ToolExecution* te = (it != app.client.tool_executions().end()) ? &it->second : nullptr;
        const std::string arg = (te && !te->args_summary.empty()) ? te->args_summary : b.tool_intent;
        const char* verb =
            (b.tool_name == "read" || b.tool_name == "read_file") ? app.t("chat.verb_read", "Read") :
            (b.tool_name == "grep" || b.tool_name == "search")    ? app.t("chat.verb_searched", "Searched") :
            (b.tool_name == "find" || b.tool_name == "glob")      ? app.t("chat.verb_found", "Found") :
                                                                    app.t("chat.verb_inspected", "Inspected");
        summary = std::string(verb) + " " + arg;
        preview_chips.clear();
    } else {
        const char* sep = app.t("chat.list_sep", ", ");
        std::string parts;
        auto append_part = [&](const std::string& p) {
            if (!parts.empty()) parts += sep;
            parts += p;
        };
        if (read_count > 0) {
            const std::string n = std::to_string(read_count);
            append_part(read_count == 1
                ? app.tf("chat.count_file_one", "{n} file", {{"n", n}})
                : app.tf("chat.count_file_other", "{n} files", {{"n", n}}));
        }
        if (search_count > 0) {
            const std::string n = std::to_string(search_count);
            append_part(search_count == 1
                ? app.tf("chat.count_search_one", "{n} search", {{"n", n}})
                : app.tf("chat.count_search_other", "{n} searches", {{"n", n}}));
        }
        if (other_count > 0) {
            const std::string n = std::to_string(other_count);
            append_part(other_count == 1
                ? app.tf("chat.count_lookup_one", "{n} lookup", {{"n", n}})
                : app.tf("chat.count_lookup_other", "{n} lookups", {{"n", n}}));
        }
        summary = any_running
            ? app.tf("chat.exploring_summary", "Exploring {items}", {{"items", parts}})
            : app.tf("chat.explored_summary",  "Explored {items}",  {{"items", parts}});
    }

    ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 3.0f}) {
        // Single whisper-quiet borderless activity line
        ETCH_BOX({
            .id = hdr_id,
            .axis = etch::Axis::Row,
            .width = etch::Sizing::fit(),
            .height = etch::Sizing::fixed(ts.pill_h - 1.0f),
            .padding = etch::Padding::xy(7.0f, 0.0f),
            .gap = 7.0f,
            .align_y = etch::AlignY::Center,
            .background = hov ? pal.hover : etch::Color::transparent(),
            .radius = etch::Corners::all(6.0f),
        }) {
            if (any_running) {
                etch::icon({
                    .icon_id = app.icons.loader_arc,
                    .size_px = 13.0f,
                    .color = pal.accent,
                    .rotation_rad = app.elapsed_s * 5.2f,
                });
            } else {
                etch::icon({
                    .icon_id = (search_count > 0 && read_count == 0) ? app.icons.regex_search : app.icons.eye_open,
                    .size_px = 13.5f,
                    .color = (open || hov) ? pal.secondary : pal.text_muted,
                });
            }

            etch::text(etch::str_cstr(summary.c_str()), {
                .color = (open || hov) ? pal.text_primary : pal.text_muted,
                .font_size_px = ts.ui_sm,
                .font_id = app.fonts.medium,
                .wrap = false,
            });

            // Quiet inline file chips when collapsed
            if (!open && !preview_chips.empty()) {
                for (const auto& chip : preview_chips) {
                    ETCH_BOX({
                        .padding = etch::Padding::xy(7.0f, 2.0f),
                        .background = pal.dark ? etch::Color::rgba8(244, 238, 226, 10) : etch::Color::rgba8(28, 25, 21, 9),
                        .radius = etch::Corners::all(4.0f),
                    }) {
                        etch::text(etch::str_cstr(chip.c_str()), {
                            .color = pal.text_muted,
                            .font_size_px = ts.meta,
                            .font_id = app.fonts.mono,
                            .wrap = false,
                        });
                    }
                }
            }

            etch::icon({
                .icon_id = app.icons.chevron_r,
                .size_px = 11.0f,
                .color = pal.text_faint,
                .rotation_rad = chev_rad,
            });
        }
        if (hov) etch::set_cursor(etch::Cursor::Hand);
        if (etch::clicked(hdr_id)) {
            app.toggle_disclosure(key, false);
        }

        // Smooth expandable timeline stem (zero card bloat; click any row to inspect in Workbench!)
        ETCH_COLLAPSE({
            .id = etch::id_index(hdr_id, 2),
            .open = open,
            .spring = kMotionFluid,
            .fade_content = true,
        }) {
            ETCH_BOX({
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .padding = {.left = 12.0f, .right = 4.0f, .top = 3.0f, .bottom = 4.0f},
                .gap = 2.0f,
                .border_color = pal.border_subtle,
                .border_sides = {.left = 1.5f},
                .margin = {.left = 11.0f},
            }) {
                for (size_t i = start_bi; i < end_bi; ++i) {
                    const ContentBlock& b = m.blocks[i];
                    const auto it = app.client.tool_executions().find(b.tool_call_id);
                    const ToolExecution* te = (it != app.client.tool_executions().end()) ? &it->second : nullptr;
                    const bool running = te ? te->running : !b.complete;
                    const bool is_err = te ? te->is_error : false;
                    const std::string arg = (te && !te->args_summary.empty()) ? te->args_summary : b.tool_intent;
                    const uint32_t rid = etch::id_index(hdr_id, static_cast<uint32_t>(10 + (i - start_bi)));
                    const bool rh = etch::hovered(rid);

                    const uint16_t ico =
                        running ? app.icons.loader_arc :
                        is_err ? app.icons.x_circle :
                        (b.tool_name == "read" || b.tool_name == "read_file") ? app.icons.eye_open :
                        (b.tool_name == "find" || b.tool_name == "glob" || b.tool_name == "ls") ? app.icons.folder_search :
                        (b.tool_name == "web_search" || b.tool_name == "fetch") ? app.icons.globe : app.icons.regex_search;

                    ETCH_BOX({
                        .id = rid,
                        .axis = etch::Axis::Row,
                        .width = etch::Sizing::grow(),
                        .height = etch::Sizing::fixed(ts.pill_h - 1.0f),
                        .padding = etch::Padding::xy(7.0f, 0.0f),
                        .gap = 8.0f,
                        .align_y = etch::AlignY::Center,
                        .background = rh ? pal.hover : etch::Color::transparent(),
                        .radius = etch::Corners::all(5.0f),
                        .shrink_width = true,
                    }) {
                        etch::icon({
                            .icon_id = ico,
                            .size_px = 12.5f,
                            .color = is_err ? pal.danger : (running ? pal.accent : (rh ? pal.secondary : pal.text_faint)),
                            .rotation_rad = running ? (app.elapsed_s * 5.2f) : 0.0f,
                        });
                        etch::text(etch::str_cstr(b.tool_name.c_str()), {
                            .color = pal.text_faint,
                            .font_size_px = ts.meta,
                            .font_id = app.fonts.mono,
                            .wrap = false,
                        });
                        ETCH_BOX({.width = etch::Sizing::grow(), .clip_children = true, .shrink_width = true}) {
                            etch::text(etch::str_cstr(arg.c_str()), {
                                .color = rh ? pal.text_primary : pal.text_secondary,
                                .font_size_px = ts.code - 0.5f,
                                .font_id = app.fonts.mono,
                                .wrap = false,
                                .ellipsis = true,
                            });
                        }
                        if (rh && te) {
                            etch::text(app.ts("chat.inspect", "Inspect"), {
                                .color = pal.accent,
                                .font_size_px = ts.meta,
                                .font_id = app.fonts.medium,
                                .wrap = false,
                            });
                        } else if (te && te->ended_ms > te->started_ms) {
                            const std::string dur = format_duration_compact(te->ended_ms - te->started_ms);
                            etch::text(etch::str_cstr(dur.c_str()), {
                                .color = pal.text_faint,
                                .font_size_px = ts.meta,
                                .font_id = app.fonts.mono_tnum,
                                .wrap = false,
                            });
                        }
                    }
                    if (rh) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(rid) && te) {
                        app.open_workbench_tool(te->tool_call_id);
                    }
                }
            }
        }
    }
}

// ============================================================================
// 3. Card Type A — File Diff / Mutation Ledger Card (`edit` / `write` / `patch` / `ast_edit`)
// ============================================================================
void render_diff_ledger_card(App& app, const Palette& pal, const ChatMessage& m, size_t mi, size_t bi,
                             const ContentBlock& b, const ToolExecution* te) {
    const TypeScale ts = app.type();
    const std::string key = "diff_" + msg_uid(m, mi) + "_" + std::to_string(bi);
    const std::string diff_src = te ? best_diff_text(*te) : std::string{};
    const bool has_diff = looks_like_structured_diff(diff_src);
    int approx_lines = 0;
    if (has_diff) {
        approx_lines = 1 + static_cast<int>(std::count(diff_src.begin(), diff_src.end(), '\n'));
    }
    const bool default_open = has_diff && approx_lines > 0 && approx_lines <= 18;
    const bool open = app.is_disclosure_open(key, default_open);
    const bool running = te ? te->running : !b.complete;
    const bool is_err = te ? te->is_error : false;
    const std::string path = (te && !te->args_summary.empty()) ? te->args_summary : b.tool_intent;
    const auto [dir, fname] = split_dir_file(path);
    const auto [adds, dels] = count_diff_stats(diff_src);

    const uint32_t card_id = etch::id(key.c_str());
    const uint32_t hdr_id = etch::id_index(card_id, 1);
    const uint32_t wb_id = etch::id_index(card_id, 2);
    const bool hdr_hov = etch::hovered(hdr_id);
    const bool wb_hov = etch::hovered(wb_id);
    const float chev_rad = etch::spring_angle(etch::id_index(card_id, 3), open ? 1.5707963f : 0.0f, kMotionSnap);

    ETCH_BOX({
        .id = card_id,
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .background = pal.bg_surface,
        .border_color = is_err ? pal.danger : pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .clip_children = true,
        .continuous_corners = true,
    }) {
        ETCH_BOX({
            .id = hdr_id,
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fixed(ts.card_hdr_h),
            .padding = etch::Padding::xy(12.0f, 0.0f),
            .gap = 8.0f,
            .align_y = etch::AlignY::Center,
            .background = hdr_hov ? pal.bg_elevated : pal.bg_surface,
            .border_color = open ? pal.border_subtle : etch::Color::transparent(),
            .border_sides = {.bottom = open ? 1.0f : 0.0f},
        }) {
            const bool is_create = (b.tool_name == "write" || b.tool_name == "write_file" || b.tool_name == "create_file");
            etch::icon({
                .icon_id = running ? app.icons.loader_arc : (is_err ? app.icons.x_circle : (is_create ? app.icons.file_plus : app.icons.file_diff_duotone)),
                .size_px = 15.0f,
                .color = is_err ? pal.danger : (running ? pal.accent : pal.secondary),
                .rotation_rad = running ? (app.elapsed_s * 5.2f) : 0.0f,
            });
            ETCH_BOX({.axis = etch::Axis::Row, .gap = 0.0f, .align_y = etch::AlignY::Center, .shrink_width = true}) {
                if (!dir.empty()) {
                    etch::text(etch::str_cstr(dir.c_str()), {
                        .color = pal.text_faint, .font_size_px = ts.code, .font_id = app.fonts.mono, .wrap = false
                    });
                }
                etch::text(etch::str_cstr(fname.empty() ? b.tool_name.c_str() : fname.c_str()), {
                    .color = pal.text_primary, .font_size_px = ts.code, .font_id = app.fonts.mono, .wrap = false, .ellipsis = true
                });
            }

            // Tabular +N / -M diff stat counters
            if (adds > 0 || dels > 0) {
                ETCH_BOX({.axis = etch::Axis::Row, .gap = 5.0f, .align_y = etch::AlignY::Center}) {
                    if (adds > 0) {
                        char ab[16];
                        std::snprintf(ab, sizeof(ab), "+%d", adds);
                        etch::text(etch::str_cstr(ab), {
                            .color = pal.success, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                        });
                    }
                    if (dels > 0) {
                        char db[16];
                        std::snprintf(db, sizeof(db), "-%d", dels);
                        etch::text(etch::str_cstr(db), {
                            .color = pal.danger, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                        });
                    }
                }
            }

            etch::box({.width = etch::Sizing::grow()});

            if (te && te->ended_ms > te->started_ms) {
                const std::string dur = format_duration_compact(te->ended_ms - te->started_ms);
                etch::text(etch::str_cstr(dur.c_str()), {
                    .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                });
            }

            // Open in Split-Pane Workbench button
            if (te) {
                ETCH_BOX({
                    .id = wb_id,
                    .width = etch::Sizing::fixed(22.0f),
                    .height = etch::Sizing::fixed(22.0f),
                    .align_x = etch::AlignX::Center,
                    .align_y = etch::AlignY::Center,
                    .background = wb_hov ? pal.active : etch::Color::transparent(),
                    .radius = etch::Corners::all(5.0f),
                }) {
                    etch::icon({
                        .icon_id = app.icons.maximize,
                        .size_px = 11.5f,
                        .color = wb_hov ? pal.accent : pal.text_faint,
                    });
                }
                if (wb_hov) etch::set_cursor(etch::Cursor::Hand);
                etch::set_tooltip(wb_id, app.ts("chat.open_diff_wb", "Open diff in Workbench"));
                if (etch::clicked(wb_id)) {
                    app.open_workbench_tool(te->tool_call_id);
                }
            }

            etch::icon({
                .icon_id = app.icons.chevron_r,
                .size_px = 11.5f,
                .color = pal.text_faint,
                .rotation_rad = chev_rad,
            });
        }
        if (hdr_hov && !wb_hov) etch::set_cursor(etch::Cursor::Hand);
        if (etch::clicked(hdr_id) && !etch::clicked(wb_id)) {
            app.toggle_disclosure(key, default_open);
        }

        ETCH_COLLAPSE({
            .id = etch::id_index(card_id, 4),
            .open = open,
            .spring = kMotionFluid,
            .fade_content = true,
        }) {
            if (has_diff) {
                render_structured_diff(diff_src, pal, app.fonts, 36, ts.code);
            } else if (!diff_src.empty()) {
                ETCH_BOX({.width = etch::Sizing::grow(), .padding = etch::Padding::xy(12.0f, 8.0f), .background = pal.bg_code}) {
                    etch::text(etch::str_cstr(diff_src.c_str()), {
                        .color = pal.text_secondary, .font_size_px = ts.code, .font_id = app.fonts.mono, .wrap = true, .selectable = true
                    });
                }
            } else if (!b.tool_args_json.empty()) {
                ETCH_BOX({.width = etch::Sizing::grow(), .padding = etch::Padding::xy(12.0f, 8.0f), .background = pal.bg_code}) {
                    etch::text(etch::str_cstr(b.tool_args_json.c_str()), {
                        .color = pal.text_muted, .font_size_px = ts.code, .font_id = app.fonts.mono, .wrap = true
                    });
                }
            }
        }
    }
}

// ============================================================================
// 4. Card Type B — Live Command Execution Card (`bash` / `shell`)
// ============================================================================
void render_shell_card(App& app, const Palette& pal, const ChatMessage& m, size_t mi, size_t bi,
                       const ContentBlock& b, const ToolExecution* te, bool default_open) {
    const TypeScale ts = app.type();
    const std::string key = "sh_" + msg_uid(m, mi) + "_" + std::to_string(bi);
    const bool open = app.is_disclosure_open(key, default_open);
    const bool running = te ? te->running : !b.complete;
    const bool is_err = te ? te->is_error : false;
    const std::string cmd = (te && !te->args_summary.empty()) ? te->args_summary : b.tool_intent;

    const uint32_t card_id = etch::id(key.c_str());
    const uint32_t hdr_id = etch::id_index(card_id, 1);
    const uint32_t cp_id = etch::id_index(card_id, 2);
    const uint32_t wb_id = etch::id_index(card_id, 3);
    const bool hdr_hov = etch::hovered(hdr_id);
    const bool cp_hov = etch::hovered(cp_id);
    const bool wb_hov = etch::hovered(wb_id);
    const float chev_rad = etch::spring_angle(etch::id_index(card_id, 4), open ? 1.5707963f : 0.0f, kMotionSnap);

    // Instrument surface styling (stays crisp dark graphite even in Light themes)
    const etch::Color hdr_bg = pal.dark ? pal.bg_surface : etch::Color::rgb_hex(0x1E1E22);
    const etch::Color hdr_bg_hov = pal.dark ? pal.bg_elevated : etch::Color::rgb_hex(0x26262C);
    const etch::Color hdr_fg = pal.dark ? pal.text_primary : etch::Color::rgb_hex(0xF1ECE2);
    const etch::Color hdr_muted = pal.dark ? pal.text_faint : etch::Color::rgb_hex(0x8D877E);
    const etch::Color border_col = is_err ? pal.danger : (pal.dark ? pal.border_subtle : etch::Color::rgba8(24, 22, 18, 44));

    const std::string copy_key = "cp_cmd_" + key;
    const bool is_copied = (app.copied_key == copy_key);

    ETCH_BOX({
        .id = card_id,
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .background = pal.dark ? pal.bg_code : etch::Color::rgb_hex(0x161619),
        .border_color = border_col,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .clip_children = true,
        .continuous_corners = true,
    }) {
        ETCH_BOX({
            .id = hdr_id,
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fixed(ts.card_hdr_h),
            .padding = etch::Padding::xy(12.0f, 0.0f),
            .gap = 8.0f,
            .align_y = etch::AlignY::Center,
            .background = hdr_hov ? hdr_bg_hov : hdr_bg,
            .border_color = open ? etch::Color::rgba8(255, 255, 255, 16) : etch::Color::transparent(),
            .border_sides = {.bottom = open ? 1.0f : 0.0f},
        }) {
            etch::icon({
                .icon_id = running ? app.icons.loader_arc : (is_err ? app.icons.x_circle : app.icons.terminal_check),
                .size_px = 14.5f,
                .color = is_err ? pal.danger : (running ? pal.accent : pal.success),
                .rotation_rad = running ? (app.elapsed_s * 5.2f) : 0.0f,
            });
            etch::text(etch::str_lit("$"), {
                .color = pal.accent, .font_size_px = ts.code, .font_id = app.fonts.mono, .wrap = false
            });
            etch::text(etch::str_cstr(cmd.c_str()), {
                .color = hdr_fg, .font_size_px = ts.code, .font_id = app.fonts.mono, .wrap = false, .ellipsis = true
            });

            etch::box({.width = etch::Sizing::grow()});

            if (te && te->ended_ms > te->started_ms) {
                const std::string dur = format_duration_compact(te->ended_ms - te->started_ms);
                etch::text(etch::str_cstr(dur.c_str()), {
                    .color = hdr_muted, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                });
            }

            // Copy command button with state-reactive check morph
            ETCH_BOX({
                .id = cp_id,
                .width = etch::Sizing::fixed(22.0f),
                .height = etch::Sizing::fixed(22.0f),
                .align_x = etch::AlignX::Center,
                .align_y = etch::AlignY::Center,
                .background = cp_hov ? etch::Color::rgba8(255, 255, 255, 18) : etch::Color::transparent(),
                .radius = etch::Corners::all(5.0f),
            }) {
                etch::icon({
                    .icon_id = is_copied ? app.icons.check : app.icons.copy,
                    .size_px = 11.5f,
                    .color = is_copied ? pal.success : (cp_hov ? hdr_fg : hdr_muted),
                });
            }
            if (cp_hov) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(cp_id)) {
                const etch::Rect r = etch::element_bounds(cp_id);
                app.copy_with_feedback(copy_key, cmd, {r.x + r.w * 0.5f, r.y + r.h * 0.5f});
            }

            if (te) {
                ETCH_BOX({
                    .id = wb_id,
                    .width = etch::Sizing::fixed(22.0f),
                    .height = etch::Sizing::fixed(22.0f),
                    .align_x = etch::AlignX::Center,
                    .align_y = etch::AlignY::Center,
                    .background = wb_hov ? etch::Color::rgba8(255, 255, 255, 18) : etch::Color::transparent(),
                    .radius = etch::Corners::all(5.0f),
                }) {
                    etch::icon({
                        .icon_id = app.icons.maximize,
                        .size_px = 11.5f,
                        .color = wb_hov ? pal.accent : hdr_muted,
                    });
                }
                if (wb_hov) etch::set_cursor(etch::Cursor::Hand);
                etch::set_tooltip(wb_id, app.ts("chat.focus_term_wb", "Focus terminal in Workbench"));
                if (etch::clicked(wb_id)) {
                    app.open_workbench_tool(te->tool_call_id);
                }
            }

            etch::icon({
                .icon_id = app.icons.chevron_r,
                .size_px = 11.5f,
                .color = hdr_muted,
                .rotation_rad = chev_rad,
            });
        }
        if (hdr_hov && !cp_hov && !wb_hov) etch::set_cursor(etch::Cursor::Hand);
        if (etch::clicked(hdr_id) && !etch::clicked(cp_id) && !etch::clicked(wb_id)) {
            app.toggle_disclosure(key, default_open);
        }

        ETCH_COLLAPSE({
            .id = etch::id_index(card_id, 5),
            .open = open,
            .spring = kMotionFluid,
            .fade_content = true,
        }) {
            if (te) render_bash_terminal_block(*te, pal, app.fonts, ts);
        }
    }
}

// ============================================================================
// 5. Card Type C — Execution Plan / Checklist Card (`todo_write` / `plan`)
// ============================================================================
void render_plan_card(App& app, const Palette& pal, const ChatMessage& m, size_t mi, size_t bi,
                      const ContentBlock& b, const ToolExecution* te) {
    const TypeScale ts = app.type();
    const std::vector<PlanStep> steps = parse_plan_steps(b.tool_args_json, te ? te->output_text : "");
    if (steps.empty()) return;

    int done = 0;
    for (const auto& s : steps) {
        if (s.status == "completed" || s.status == "done") ++done;
    }
    const float frac = static_cast<float>(done) / static_cast<float>(steps.size());
    const std::string key = "plan_" + msg_uid(m, mi) + "_" + std::to_string(bi);
    const bool open = app.is_disclosure_open(key, true);
    const uint32_t card_id = etch::id(key.c_str());
    const uint32_t hdr_id = etch::id_index(card_id, 1);
    const bool hdr_hov = etch::hovered(hdr_id);
    const float chev_rad = etch::spring_angle(etch::id_index(card_id, 2), open ? 1.5707963f : 0.0f, kMotionSnap);

    char cnt[24];
    std::snprintf(cnt, sizeof(cnt), "%d / %zu", done, steps.size());

    ETCH_BOX({
        .id = card_id,
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .background = pal.bg_surface,
        .border_color = pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .clip_children = true,
        .continuous_corners = true,
    }) {
        ETCH_BOX({
            .id = hdr_id,
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fixed(ts.card_hdr_h),
            .padding = etch::Padding::xy(12.0f, 0.0f),
            .gap = 8.0f,
            .align_y = etch::AlignY::Center,
            .background = hdr_hov ? pal.bg_elevated : pal.bg_surface,
        }) {
            etch::icon({.icon_id = app.icons.list_checks, .size_px = 15.0f, .color = pal.accent});
            etch::text(app.ts("chat.execution_plan", "Execution Plan"), {
                .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.semibold, .wrap = false
            });
            etch::box({.width = etch::Sizing::grow()});
            etch::text(etch::str_cstr(cnt), {
                .color = (done == static_cast<int>(steps.size())) ? pal.success : pal.text_muted,
                .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
            });
            etch::icon({
                .icon_id = app.icons.chevron_r,
                .size_px = 11.0f,
                .color = pal.text_faint,
                .rotation_rad = chev_rad,
            });
        }
        if (hdr_hov) etch::set_cursor(etch::Cursor::Hand);
        if (etch::clicked(hdr_id)) app.toggle_disclosure(key, true);

        // Ultra-thin architectural hairline progress bar (2px)
        ETCH_BOX({
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fixed(2.0f),
            .background = pal.border_subtle,
        }) {
            etch::box({
                .width = etch::Sizing::percent(std::clamp(frac, 0.0f, 1.0f)),
                .height = etch::Sizing::grow(),
                .background = (done == static_cast<int>(steps.size())) ? pal.success : pal.accent,
            });
        }

        ETCH_COLLAPSE({
            .id = etch::id_index(card_id, 3),
            .open = open,
            .spring = kMotionFluid,
            .fade_content = true,
        }) {
            ETCH_BOX({
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .padding = etch::Padding::xy(12.0f, 8.0f),
                .gap = 5.0f,
            }) {
                for (const auto& s : steps) {
                    const bool is_done = (s.status == "completed" || s.status == "done");
                    const bool is_active = (s.status == "in_progress" || s.status == "running");
                    ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
                        if (is_done) {
                            etch::icon({.icon_id = app.icons.check_circle, .size_px = 13.0f, .color = pal.success});
                        } else if (is_active) {
                            etch::icon({
                                .icon_id = app.icons.loader_arc,
                                .size_px = 13.0f,
                                .color = pal.accent,
                                .rotation_rad = app.elapsed_s * 5.2f,
                            });
                        } else {
                            etch::box({
                                .width = etch::Sizing::fixed(11.0f),
                                .height = etch::Sizing::fixed(11.0f),
                                .border_color = pal.border_strong,
                                .border_width = 1.3f,
                                .radius = etch::Corners::all(6.0f),
                                .margin = {.left = 1.0f, .right = 1.0f},
                            });
                        }
                        etch::text(etch::str_cstr(s.label.c_str()), {
                            .color = is_done ? pal.text_muted : (is_active ? pal.text_primary : pal.text_secondary),
                            .font_size_px = ts.ui_sm,
                            .font_id = is_active ? app.fonts.medium : app.fonts.body,
                            .wrap = true,
                        });
                    }
                }
            }
        }
    }
}

// ============================================================================
// 6. Card Type D — Live Subagent Delegation Card (`task` / `subagent`)
// ============================================================================
void render_subagent_inline_card(App& app, const Palette& pal, const ChatMessage& m, size_t mi, size_t bi,
                                 const ContentBlock& b, const ToolExecution* te) {
    const TypeScale ts = app.type();
    const std::string key = "sub_" + msg_uid(m, mi) + "_" + std::to_string(bi);
    const uint32_t card_id = etch::id(key.c_str());
    const bool hov = etch::hovered(card_id);

    // Match live subagent telemetry spawned by this tool call so status & current tool update in real time!
    const SubagentInfo* live_sa = nullptr;
    uint32_t matched_count = 0;
    uint32_t matched_running = 0;
    uint32_t matched_failed = 0;
    uint64_t matched_tokens = 0;
    uint32_t matched_tools = 0;
    if (!b.tool_call_id.empty()) {
        for (const auto& sa : app.client.subagents()) {
            if (sa.parent_tool_call_id == b.tool_call_id) {
                ++matched_count;
                matched_tokens += sa.tokens;
                matched_tools += sa.tool_count;
                const bool sa_run = (sa.status == "running" || sa.status == "active" || sa.status == "pending");
                if (sa_run) {
                    ++matched_running;
                    live_sa = &sa;
                } else {
                    if (sa.status == "failed" || sa.status == "error" || sa.status == "aborted") ++matched_failed;
                    if (!live_sa) live_sa = &sa;
                }
            }
        }
    }

    const bool running = (matched_count > 0) ? (matched_running > 0) : (te ? te->running : !b.complete);
    const bool is_err = (matched_count > 0 && matched_running == 0) ? (matched_failed > 0) : (te ? te->is_error : false);
    const std::string title =
        (live_sa && !live_sa->description.empty()) ? live_sa->description :
        (te && !te->args_summary.empty()) ? te->args_summary :
        (!b.tool_intent.empty() ? b.tool_intent : app.t("chat.background_agent", "Background Agent"));

    ETCH_BOX({
        .id = card_id,
        .axis = etch::Axis::Row,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::xy(12.0f, 9.0f),
        .gap = 10.0f,
        .align_y = etch::AlignY::Center,
        .background = hov ? pal.bg_elevated : pal.bg_surface,
        .border_color = running ? pal.accent_border : pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusMd),
        .continuous_corners = true,
    }) {
        ETCH_BOX({
            .width = etch::Sizing::fixed(28.0f),
            .height = etch::Sizing::fixed(28.0f),
            .align_x = etch::AlignX::Center,
            .align_y = etch::AlignY::Center,
            .background = running ? pal.accent_soft : pal.bg_canvas,
            .border_color = running ? pal.accent_border : pal.border_subtle,
            .border_width = 1.0f,
            .radius = etch::Corners::all(8.0f),
            .continuous_corners = true,
        }) {
            etch::icon({
                .icon_id = running ? app.icons.loader_arc : (is_err ? app.icons.x_circle : app.icons.agents_duotone),
                .size_px = 14.5f,
                .color = is_err ? pal.danger : (running ? pal.accent : pal.success),
                .rotation_rad = running ? (app.elapsed_s * 5.2f) : 0.0f,
            });
        }
        ETCH_BOX({.axis = etch::Axis::Col, .width = etch::Sizing::grow(), .gap = 2.0f, .shrink_width = true}) {
            ETCH_BOX({.axis = etch::Axis::Row, .gap = 6.0f, .align_y = etch::AlignY::Center}) {
                etch::text(etch::str_cstr(title.c_str()), {
                    .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.semibold, .wrap = false, .ellipsis = true
                });
                etch::text(running ? app.ts("status.running", "running") : (is_err ? app.ts("status.failed", "failed") : app.ts("status.completed", "completed")), {
                    .color = is_err ? pal.danger : (running ? pal.accent : pal.success),
                    .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                });
            }
            std::string subline;
            if (live_sa && running && !live_sa->current_tool.empty()) {
                subline = live_sa->current_tool + (live_sa->current_tool_args.empty() ? "" : (" · " + live_sa->current_tool_args));
            } else if (live_sa && !live_sa->last_intent.empty()) {
                subline = live_sa->last_intent;
            } else if (matched_count > 0 && (matched_tokens > 0 || matched_tools > 0)) {
                subline = format_compact_tokens(matched_tokens) + " tok · " + std::to_string(matched_tools) + " tools";
            } else if (te && !te->output_text.empty()) {
                subline = te->output_text;
            }
            if (!subline.empty()) {
                etch::text(etch::str_cstr(subline.c_str()), {
                    .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = (live_sa && running && !live_sa->current_tool.empty()) ? app.fonts.mono : app.fonts.body,
                    .wrap = false, .ellipsis = true
                });
            }
        }
        etch::text(app.ts("chat.workbench_link", "Workbench →"), {
            .color = hov ? pal.accent : pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.medium, .wrap = false
        });
    }
    if (hov) etch::set_cursor(etch::Cursor::Hand);
    if (etch::clicked(card_id)) {
        if (live_sa) app.selected_subagent_id = live_sa->id;
        app.inspector_tab = InspectorTab::Agents;
        app.inspector_open = true;
        if (!app.showcase_mode) app.client.refresh_subagents();
    }
}

// ============================================================================
// 7. Generic Fallback Tool Row (compact minimal collapsible for MCP/custom tools)
// ============================================================================
void render_generic_tool_card(App& app, const Palette& pal, const ChatMessage& m, size_t mi, size_t bi,
                              const ContentBlock& b, const ToolExecution* te) {
    const TypeScale ts = app.type();
    const std::string key = "tool_" + msg_uid(m, mi) + "_" + std::to_string(bi);
    const bool open = app.is_disclosure_open(key, false);
    const bool running = te ? te->running : !b.complete;
    const bool is_err = te ? te->is_error : false;
    const std::string arg = (te && !te->args_summary.empty()) ? te->args_summary : b.tool_intent;

    const uint32_t card_id = etch::id(key.c_str());
    const uint32_t hdr_id = etch::id_index(card_id, 1);
    const bool hdr_hov = etch::hovered(hdr_id);
    const float chev_rad = etch::spring_angle(etch::id_index(card_id, 2), open ? 1.5707963f : 0.0f, kMotionSnap);

    ETCH_BOX({
        .id = card_id,
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .background = pal.bg_surface,
        .border_color = is_err ? pal.danger : pal.border_subtle,
        .border_width = 1.0f,
        .radius = etch::Corners::all(kRadiusSm),
        .clip_children = true,
        .continuous_corners = true,
    }) {
        ETCH_BOX({
            .id = hdr_id,
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fixed(ts.row_h),
            .padding = etch::Padding::xy(10.0f, 0.0f),
            .gap = 8.0f,
            .align_y = etch::AlignY::Center,
            .background = hdr_hov ? pal.bg_elevated : pal.bg_surface,
        }) {
            etch::icon({
                .icon_id = running ? app.icons.loader_arc : (is_err ? app.icons.x_circle : app.icons.blocks),
                .size_px = 13.5f,
                .color = is_err ? pal.danger : (running ? pal.accent : pal.secondary),
                .rotation_rad = running ? (app.elapsed_s * 5.2f) : 0.0f,
            });
            etch::text(etch::str_cstr(b.tool_name.c_str()), {
                .color = pal.text_primary, .font_size_px = ts.code, .font_id = app.fonts.mono, .wrap = false
            });
            if (!arg.empty()) {
                etch::text(etch::str_cstr(arg.c_str()), {
                    .color = pal.text_muted, .font_size_px = ts.code - 0.5f, .font_id = app.fonts.mono, .wrap = false, .ellipsis = true
                });
            }
            etch::box({.width = etch::Sizing::grow()});
            if (te && te->ended_ms > te->started_ms) {
                const std::string dur = format_duration_compact(te->ended_ms - te->started_ms);
                etch::text(etch::str_cstr(dur.c_str()), {
                    .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                });
            }
            etch::icon({.icon_id = app.icons.chevron_r, .size_px = 11.0f, .color = pal.text_faint, .rotation_rad = chev_rad});
        }
        if (hdr_hov) etch::set_cursor(etch::Cursor::Hand);
        if (etch::clicked(hdr_id)) app.toggle_disclosure(key, false);

        ETCH_COLLAPSE({
            .id = etch::id_index(card_id, 3),
            .open = open,
            .spring = kMotionFluid,
            .fade_content = true,
        }) {
            const std::string& body = (te && !te->output_text.empty()) ? te->output_text : b.tool_args_json;
            ETCH_BOX({.width = etch::Sizing::grow(), .padding = etch::Padding::xy(10.0f, 8.0f), .background = pal.bg_code}) {
                etch::text(etch::str_cstr(body.c_str()), {
                    .color = pal.text_secondary, .font_size_px = ts.code, .font_id = app.fonts.mono, .wrap = true, .selectable = true
                });
            }
        }
    }
}

// Callback injected into Inlay Mermaid headers ("Workbench" expand action!)
struct DiagramHeaderCtx {
    App* app = nullptr;
    const Palette* pal = nullptr;
};

void diagram_header_cb(uint32_t block_index, uint32_t /*row_id*/, EtchStr /*language*/, EtchStr body, void* user_data) {
    auto* hc = static_cast<DiagramHeaderCtx*>(user_data);
    if (!hc || !hc->app || !hc->pal || body.length == 0) return;
    const uint32_t btn_id = etch::id_index(etch::id("inlay_diag_wb"), block_index);
    const bool hov = etch::hovered(btn_id);
    ETCH_BOX({
        .id = btn_id,
        .axis = etch::Axis::Row,
        .height = etch::Sizing::fixed(20.0f),
        .padding = etch::Padding::xy(6.0f, 0.0f),
        .gap = 4.0f,
        .align_y = etch::AlignY::Center,
        .background = hov ? hc->pal->hover : etch::Color::transparent(),
        .radius = etch::Corners::all(4.0f),
    }) {
        etch::icon({
            .icon_id = hc->app->icons.maximize,
            .size_px = 10.5f,
            .color = hov ? hc->pal->accent : hc->pal->text_muted,
        });
        etch::text(hc->app->ts("chat.workbench", "Workbench"), {
            .color = hov ? hc->pal->text_primary : hc->pal->text_muted,
            .font_size_px = hc->app->type().meta,
            .font_id = hc->app->fonts.medium,
            .wrap = false,
        });
    }
    if (hov) etch::set_cursor(etch::Cursor::Hand);
    if (etch::clicked(btn_id)) {
        hc->app->open_workbench_diagram(std::string(body.chars, body.length));
    }
}

void build_welcome_stage(App& app) {
    const Palette& pal = app.palette();
    const TypeScale ts = app.type();

    ETCH_BOX({
        .id = etch::id("welcome_stage"),
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::grow(),
        .align_x = etch::AlignX::Center,
        .align_y = etch::AlignY::Center,
        .clip_children = true,
        .shrink_width = true,
        .shrink_height = true,
    }) {
        if (app.ambient_shader && app.config.prefs().particle_density != ParticleDensity::Off) {
            const etch::Rect b = etch::element_bounds(etch::id("welcome_stage"));
            const float draw_w = (b.w > 64.0f) ? b.w : 1100.0f;
            const float draw_h = (b.h > 64.0f) ? b.h : 640.0f;
            // Half-resolution soft ambient surface cuts GPU texture RAM & fragment fill by 4x identically!
            const uint32_t sw = static_cast<uint32_t>(std::clamp(draw_w * 0.5f, 256.0f, 640.0f));
            const uint32_t sh = static_cast<uint32_t>(std::clamp(draw_h * 0.5f, 160.0f, 400.0f));
            const etch::Vec2 ptr = etch::pointer_pos();
            const float u = (b.w > 1.0f && std::isfinite(ptr.x)) ? std::clamp((ptr.x - b.x) / b.w, 0.0f, 1.0f) : 0.5f;
            const float v = (b.h > 1.0f && std::isfinite(ptr.y)) ? std::clamp((ptr.y - b.y) / b.h, 0.0f, 1.0f) : 0.45f;
            const float p_str = (app.config.prefs().particle_density == ParticleDensity::Subtle) ? 0.55f : 1.0f;

            etch::ShaderSurfaceEvalDesc ev{};
            ev.width = sw;
            ev.height = sh;
            ev.params[0] = {pal.shader_base[0], pal.shader_base[1], pal.shader_base[2], pal.shader_base[3]};
            ev.params[1] = {pal.shader_accent[0], pal.shader_accent[1], pal.shader_accent[2], app.client.state().is_streaming ? 0.85f : 0.25f};
            ev.params[2] = {pal.shader_secondary[0], pal.shader_secondary[1], pal.shader_secondary[2], 0.0f};
            ev.params[3] = {u, v, p_str, 0.0f};
            if (auto* tex = etch::eval_shader_surface(app.ctx, app.ambient_shader, ev)) {
                etch::draw_image({
                    .texture = tex,
                    .rect = {0.0f, 0.0f, draw_w, draw_h},
                });
            }
        }

        ETCH_BOX({
            .axis = etch::Axis::Col,
            .width = etch::Sizing::fit(0.0f, 640.0f),
            .padding = etch::Padding::xy(24.0f, 16.0f),
            .gap = 12.0f,
            .align_x = etch::AlignX::Center,
        }) {
            // Luxury frameless editorial serif OMP masthead (flanked by brass architectural hairlines — zero boxed icon!)
            ETCH_BOX({
                .axis = etch::Axis::Row,
                .gap = 12.0f,
                .align_x = etch::AlignX::Center,
                .align_y = etch::AlignY::Center,
                .margin = {.bottom = 4.0f},
            }) {
                etch::box({
                    .width = etch::Sizing::fixed(36.0f),
                    .height = etch::Sizing::fixed(1.0f),
                    .background = pal.accent_border,
                });
                etch::icon({.icon_id = app.icons.logo, .size_px = 18.0f, .color = pal.accent});
                etch::text(etch::str_lit("OMP"), {
                    .color = pal.accent,
                    .font_size_px = ts.display + 14.0f,
                    .font_id = app.fonts.serif,
                    .letter_spacing = 3.2f,
                    .wrap = false,
                });
                etch::box({
                    .width = etch::Sizing::fixed(36.0f),
                    .height = etch::Sizing::fixed(1.0f),
                    .background = pal.accent_border,
                });
            }

            etch::text(app.ts("welcome.headline", "What are we building?"), {
                .color = pal.text_primary,
                .font_size_px = ts.display,
                .font_id = app.fonts.display,
                .letter_spacing = app.is_cjk_locale() ? 0.0f : -0.6f,
                .wrap = false,
            });

            const std::string ws = fs::path(app.config.prefs().active_workspace).filename().string();
            const std::string model_label = app.client.state().model.name.empty()
                ? app.t("welcome.configure_provider", "Configure provider in Settings")
                : app.client.state().model.name;
            const std::string sub = (ws.empty() ? app.t("common.workspace", "Workspace") : ws) + "  ·  " + model_label;
            etch::text(etch::str_cstr(sub.c_str()), {
                .color = pal.text_muted,
                .font_size_px = ts.code,
                .font_id = app.fonts.mono,
                .wrap = false,
            });

            if (app.client.status() == ConnectionStatus::Error) {
                ETCH_BOX({
                    .axis = etch::Axis::Row,
                    .padding = etch::Padding::xy(14.0f, 9.0f),
                    .gap = 10.0f,
                    .align_y = etch::AlignY::Center,
                    .background = pal.danger_soft,
                    .border_color = pal.danger,
                    .border_width = 1.0f,
                    .radius = etch::Corners::all(kRadiusMd),
                    .continuous_corners = true,
                }) {
                    etch::icon({.icon_id = app.icons.alert, .size_px = 15.0f, .color = pal.danger});
                    etch::text(etch::str_cstr(app.client.status_error().c_str()), {
                        .color = pal.text_primary,
                        .font_size_px = ts.ui_sm,
                        .font_id = app.fonts.body,
                        .wrap = true,
                    });
                    if (etch::button({.id = etch::id("welcome_open_engine"), .label = app.ts("welcome.configure_btn", "Configure"), .height = etch::Sizing::fixed(26.0f)})) {
                        app.settings_open = true;
                        app.settings_tab = SettingsTab::Engine;
                    }
                }
            }

            struct Starter { const char* label; const char* prompt; uint16_t ico; };
            const Starter starters[3] = {
                {app.t("welcome.starter1.label", "Map codebase architecture"),
                 app.t("welcome.starter1.prompt", "Inspect this repository's structure and explain the core architecture concisely."),
                 app.icons.layers},
                {app.t("welcome.starter2.label", "Review uncommitted diff"),
                 app.t("welcome.starter2.prompt", "Review current git status and uncommitted changes, focusing on bugs and edge cases."),
                 app.icons.git_compare},
                {app.t("welcome.starter3.label", "Run build & tests"),
                 app.t("welcome.starter3.prompt", "Check how this project builds and run its test suite, summarizing any issues."),
                 app.icons.terminal},
            };

            ETCH_BOX({
                .axis = etch::Axis::Row,
                .padding = {.top = 10.0f},
                .gap = 8.0f,
                .align_x = etch::AlignX::Center,
                .wrap = true,
            }) {
                for (uint32_t i = 0; i < 3; ++i) {
                    const uint32_t sid = etch::id_index(etch::id("welcome_starter"), i);
                    const bool hov = etch::hovered(sid);
                    ETCH_BOX({
                        .id = sid,
                        .axis = etch::Axis::Row,
                        .height = etch::Sizing::fixed(ts.row_h + 2.0f),
                        .padding = etch::Padding::xy(14.0f, 0.0f),
                        .gap = 8.0f,
                        .align_y = etch::AlignY::Center,
                        .background = hov ? pal.bg_elevated : pal.bg_surface,
                        .border_color = hov ? pal.border_strong : pal.border_subtle,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(kRadiusPill),
                        .continuous_corners = true,
                        .hover_scale = 1.015f,
                        .press_scale = 0.97f,
                        .transition = kMotionSnap,
                    }) {
                        etch::icon({.icon_id = starters[i].ico, .size_px = 14.0f, .color = hov ? pal.accent : pal.text_muted});
                        etch::text(etch::str_cstr(starters[i].label), {
                            .color = hov ? pal.text_primary : pal.text_secondary,
                            .font_size_px = ts.ui,
                            .font_id = app.fonts.medium,
                            .wrap = false,
                        });
                    }
                    if (hov) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(sid)) {
                        const size_t len = std::strlen(starters[i].prompt);
                        std::memcpy(app.composer_buf, starters[i].prompt, len + 1);
                        app.trigger_send(false);
                    }
                }
            }
        }
    }
}

} // namespace

void build_chat_view(App& app) {
    bool has_visible_messages = false;
    for (const auto& m : app.client.messages()) {
        if (!m.hidden && m.role != "toolResult" && m.role != "system" && m.role != "developer") {
            has_visible_messages = true;
            break;
        }
    }
    if (!has_visible_messages && !app.client.state().is_streaming) {
        build_welcome_stage(app);
        return;
    }

    const Palette& pal = app.palette();
    const TypeScale ts = app.type();
    const inlay::DocStyle doc_style = pal.to_inlay_style(app.fonts.body, app.fonts.bold, app.fonts.mono);
    const uint32_t scroll_id = etch::id("chat_scroll");
    DiagramHeaderCtx diag_ctx{&app, &pal};

    // Non-scrolling stage wrapper holds both the sticky-tail scroll viewport and the floating jump-to-bottom pill
    ETCH_BOX({
        .id = etch::id("chat_stage_wrap"),
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::grow(),
        .shrink_width = true,
        .shrink_height = true,
    }) {
        ETCH_BOX({
            .id = scroll_id,
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::grow(),
            .padding = {.left = 24.0f, .right = 24.0f, .top = 20.0f, .bottom = 24.0f},
            .align_x = etch::AlignX::Center,
            .clip_children = true,
            .scroll_y = true,
            .shrink_width = true,
            .shrink_height = true,
            .scroll_fade_px = 28.0f,
            .stick_to_bottom = true,
        }) {
        const etch::Rect stage_b = etch::element_bounds(scroll_id);
        const float col_w = (stage_b.w > 120.0f)
            ? std::min(kReadingMaxW, stage_b.w - 48.0f)
            : 760.0f;

        ETCH_BOX({
            .id = etch::id("chat_reading_col"),
            .axis = etch::Axis::Col,
            .width = etch::Sizing::fixed(col_w),
            .gap = 16.0f,
        }) {
            const auto& msgs = app.client.messages();
            for (size_t mi = 0; mi < msgs.size(); ++mi) {
                const ChatMessage& m = msgs[mi];
                if (m.hidden || m.role == "toolResult" || m.role == "system" || m.role == "developer") continue;

                if (m.role == "user") {
                    ETCH_BOX({
                        .axis = etch::Axis::Row,
                        .width = etch::Sizing::grow(),
                        .justify = etch::Justify::End,
                    }) {
                        ETCH_BOX({
                            .axis = etch::Axis::Col,
                            .width = etch::Sizing::fit(0.0f, col_w * 0.78f),
                            .padding = etch::Padding::xy(15.0f, 11.0f),
                            .gap = 6.0f,
                            .background = pal.bg_surface,
                            .border_color = pal.border_subtle,
                            .border_width = 1.0f,
                            .radius = etch::Corners::all(kRadiusMd),
                            .continuous_corners = true,
                            .top_rim_alpha = pal.dark ? 0.08f : 0.0f,
                        }) {
                            for (const auto& b : m.blocks) {
                                if (b.kind == ContentBlockKind::Text && !b.text.empty()) {
                                    etch::text(etch::str_cstr(b.text.c_str()), {
                                        .color = pal.text_primary,
                                        .font_size_px = app.config.prefs().font_size_px,
                                        .font_id = app.fonts.body,
                                        .wrap = true,
                                        .selectable = true,
                                        .line_height_px = ts.body * 1.54f,
                                    });
                                } else if (b.kind == ContentBlockKind::Image) {
                                    ETCH_BOX({
                                        .axis = etch::Axis::Row,
                                        .padding = etch::Padding::xy(8.0f, 4.0f),
                                        .gap = 6.0f,
                                        .align_y = etch::AlignY::Center,
                                        .background = pal.bg_elevated,
                                        .radius = etch::Corners::all(6.0f),
                                    }) {
                                        etch::icon({.icon_id = app.icons.image_frame, .size_px = 12.5f, .color = pal.accent});
                                        etch::text(etch::str_cstr(b.mime_type.c_str()), {
                                            .color = pal.text_secondary, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                                        });
                                    }
                                }
                            }
                        }
                    }
                    continue;
                }

                if (m.role == "notice" || m.role == "custom" || m.role == "compactionSummary" || m.role == "branchSummary") {
                    ETCH_BOX({
                        .axis = etch::Axis::Row,
                        .width = etch::Sizing::grow(),
                        .padding = etch::Padding::xy(12.0f, 8.0f),
                        .gap = 8.0f,
                        .align_y = etch::AlignY::Center,
                        .background = m.is_error ? pal.danger_soft : pal.selected_subtle,
                        .border_color = m.is_error ? pal.danger : pal.border_subtle,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(kRadiusSm),
                        .continuous_corners = true,
                    }) {
                        etch::icon({.icon_id = m.is_error ? app.icons.alert : app.icons.compress,
                                    .size_px = 13.5f, .color = m.is_error ? pal.danger : pal.accent});
                        if (!m.blocks.empty()) {
                            etch::text(etch::str_cstr(m.blocks[0].text.c_str()), {
                                .color = pal.text_secondary, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true
                            });
                        }
                    }
                    continue;
                }

                // Assistant turn: borderless editorial reading flow + zero-card traces + bespoke action cards
                ETCH_BOX({
                    .axis = etch::Axis::Col,
                    .width = etch::Sizing::grow(),
                    .gap = 9.0f,
                }) {
                    std::string plain_copy_accum;
                    for (size_t bi = 0; bi < m.blocks.size(); ) {
                        const ContentBlock& b = m.blocks[bi];

                        if (b.kind == ContentBlockKind::Thinking || b.kind == ContentBlockKind::RedactedThinking) {
                            render_thinking_trace(app, pal, m, mi, bi, b);
                            ++bi;
                            continue;
                        }

                        if (b.kind == ContentBlockKind::ToolCall) {
                            // Group consecutive read-only exploration steps into ONE borderless activity stream!
                            if (is_exploration_tool(b.tool_name)) {
                                size_t end_bi = bi + 1;
                                while (end_bi < m.blocks.size() &&
                                       m.blocks[end_bi].kind == ContentBlockKind::ToolCall &&
                                       is_exploration_tool(m.blocks[end_bi].tool_name)) {
                                    ++end_bi;
                                }
                                render_exploration_group(app, pal, m, mi, bi, end_bi);
                                bi = end_bi;
                                continue;
                            }

                            const auto it = app.client.tool_executions().find(b.tool_call_id);
                            const ToolExecution* te = (it != app.client.tool_executions().end()) ? &it->second : nullptr;

                            if (is_plan_tool(b.tool_name)) {
                                render_plan_card(app, pal, m, mi, bi, b, te);
                            } else if (is_mutation_tool(b.tool_name)) {
                                render_diff_ledger_card(app, pal, m, mi, bi, b, te);
                            } else if (is_shell_tool(b.tool_name)) {
                                render_shell_card(app, pal, m, mi, bi, b, te, (mi + 1 == msgs.size()));
                            } else if (is_subagent_tool(b.tool_name)) {
                                render_subagent_inline_card(app, pal, m, mi, bi, b, te);
                            } else {
                                render_generic_tool_card(app, pal, m, mi, bi, b, te);
                            }
                            ++bi;
                            continue;
                        }

                        if (b.kind == ContentBlockKind::Text && !b.text.empty()) {
                            plain_copy_accum += b.text;
                            const std::string doc_key = msg_uid(m, mi) + ":" + std::to_string(bi);
                            inlay::Doc* doc = app.get_or_sync_doc(doc_key, b.text, m.render_revision);
                            if (doc) {
                                inlay::DocViewDesc vd = inlay::doc_view_desc_default();
                                vd.id = etch::id(doc_key.c_str());
                                vd.width_px = col_w;
                                vd.height_px = 0.0f;
                                vd.font_size_px = app.config.prefs().font_size_px;
                                vd.block_padding_px = 2.0f;
                                vd.block_gap_px = 10.0f;
                                vd.style = &doc_style;
                                vd.show_code_line_numbers = false;
                                vd.diagram_header_fn = diagram_header_cb;
                                vd.diagram_header_user_data = &diag_ctx;
                                const inlay::DocResult dres = inlay_doc_view(doc, &vd);
                                if (dres.clicked_link.length > 0) {
                                    std::string url(dres.clicked_link.chars, dres.clicked_link.length);
                                    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
                                        open_external_url(url);
                                    }
                                }
                            }
                        }
                        ++bi;
                    }

                    // Quiet single-line turn footer with spring-pop Copy checkmark
                    if (!m.streaming && !plain_copy_accum.empty()) {
                        ETCH_BOX({
                            .axis = etch::Axis::Row,
                            .width = etch::Sizing::grow(),
                            .padding = {.top = 2.0f},
                            .gap = 8.0f,
                            .align_y = etch::AlignY::Center,
                        }) {
                            const std::string copy_key = "copy_" + msg_uid(m, mi);
                            const uint32_t c_id = etch::id(copy_key.c_str());
                            const bool c_hov = etch::hovered(c_id);
                            const bool is_copied = (app.copied_key == copy_key);
                            const float cp_scale = etch::spring(etch::id_index(c_id, 1), is_copied ? 1.15f : 1.0f, 1.0f, kMotionTactile);

                            ETCH_BOX({
                                .id = c_id,
                                .axis = etch::Axis::Row,
                                .padding = etch::Padding::xy(6.0f, 3.0f),
                                .gap = 4.0f,
                                .align_y = etch::AlignY::Center,
                                .background = c_hov ? pal.hover : etch::Color::transparent(),
                                .radius = etch::Corners::all(5.0f),
                            }) {
                                etch::icon({
                                    .icon_id = is_copied ? app.icons.check : app.icons.copy,
                                    .size_px = 11.5f,
                                    .color = is_copied ? pal.success : pal.text_faint,
                                    .scale = cp_scale,
                                });
                                etch::text(is_copied ? app.ts("common.copied", "Copied") : app.ts("common.copy", "Copy"), {
                                    .color = is_copied ? pal.success : (c_hov ? pal.text_secondary : pal.text_faint),
                                    .font_size_px = ts.meta, .font_id = app.fonts.medium, .wrap = false
                                });
                            }
                            if (c_hov) etch::set_cursor(etch::Cursor::Hand);
                            if (etch::clicked(c_id)) {
                                const etch::Rect r = etch::element_bounds(c_id);
                                app.copy_with_feedback(copy_key, plain_copy_accum, {r.x + r.w * 0.5f, r.y + r.h * 0.5f});
                            }

                            const uint32_t fk_id = etch::id_index(etch::id("msg_fork"), static_cast<uint32_t>(mi));
                            const bool fk_hov = etch::hovered(fk_id);
                            ETCH_BOX({
                                .id = fk_id,
                                .axis = etch::Axis::Row,
                                .padding = etch::Padding::xy(6.0f, 3.0f),
                                .gap = 4.0f,
                                .align_y = etch::AlignY::Center,
                                .background = fk_hov ? pal.hover : etch::Color::transparent(),
                                .radius = etch::Corners::all(5.0f),
                            }) {
                                etch::icon({.icon_id = app.icons.git_fork, .size_px = 12.0f, .color = pal.text_faint});
                                etch::text(app.ts("chat.fork", "Fork"), {
                                    .color = fk_hov ? pal.text_secondary : pal.text_faint,
                                    .font_size_px = ts.meta, .font_id = app.fonts.medium, .wrap = false
                                });
                            }
                            if (fk_hov) etch::set_cursor(etch::Cursor::Hand);
                            if (etch::clicked(fk_id) && !app.showcase_mode) {
                                app.client.fork_session();
                            }

                            etch::box({.width = etch::Sizing::grow()});

                            if (m.output_tokens > 0 || !m.model_id.empty()) {
                                std::string meta;
                                if (!m.model_id.empty()) meta = m.model_id;
                                if (m.output_tokens > 0) {
                                    if (!meta.empty()) meta += " · ";
                                    meta += format_compact_tokens(m.output_tokens) + " tok";
                                }
                                if (m.cost_usd > 0.0001) {
                                    meta += " · " + format_usd(m.cost_usd);
                                }
                                etch::text(etch::str_cstr(meta.c_str()), {
                                    .color = pal.text_faint,
                                    .font_size_px = ts.meta,
                                    .font_id = app.fonts.mono_tnum,
                                    .wrap = false,
                                });
                            }
                        }
                    }
                }
            }

            if (app.client.state().is_streaming) {
                ETCH_BOX({
                    .axis = etch::Axis::Row,
                    .padding = {.top = 4.0f},
                    .gap = 8.0f,
                    .align_y = etch::AlignY::Center,
                }) {
                    etch::icon({
                        .icon_id = app.icons.loader_arc,
                        .size_px = 12.5f,
                        .color = pal.accent,
                        .rotation_rad = app.elapsed_s * 5.2f,
                    });
                    const float wipe_x = std::fmod(app.elapsed_s * 140.0f, 220.0f) - 40.0f;
                    const std::string status_txt = app.client.state().tokens_per_second > 0.5
                        ? app.tf("chat.synthesizing_tps", "Synthesizing · {tps} tok/s",
                                 {{"tps", std::to_string(static_cast<int>(app.client.state().tokens_per_second))}})
                        : std::string(app.t("chat.thinking", "Thinking..."));
                    etch::text_spans({
                        etch::TextSpan{
                            .text = etch::str_cstr(status_txt.c_str()),
                            .color = pal.text_muted,
                            .font_size_px = ts.code,
                            .font_id = app.fonts.mono_tnum,
                            .has_wipe = true,
                            .color_end = pal.accent,
                            .wipe_x = wipe_x,
                            .wipe_feather_px = 36.0f,
                        },
                    });
                }
            }
        }
        }

        // One-shot re-pin trigger when user sends a new prompt or switches sessions/branches
        if (app.stick_chat_bottom) {
            etch::set_scroll_stuck_to_bottom(scroll_id, true);
            app.stick_chat_bottom = false;
        }
        app.last_msg_count = static_cast<uint32_t>(app.client.messages().size());

        // Floating "Jump to latest" pill whenever the user deliberately scrolled up into history
        const etch::Vec2 smax = etch::scroll_max(scroll_id);
        const etch::Vec2 soff = etch::scroll_offset(scroll_id);
        const bool away_from_bottom = !etch::scroll_stuck_to_bottom(scroll_id) &&
                                      smax.y > 40.0f && (smax.y - soff.y) > 48.0f;
        if (away_from_bottom) {
            const uint32_t jump_id = etch::id("chat_jump_bottom");
            const bool jh = etch::hovered(jump_id);
            const bool streaming = app.client.state().is_streaming;
            ETCH_BOX({
                .id = jump_id,
                .axis = etch::Axis::Row,
                .height = etch::Sizing::fixed(30.0f),
                .padding = etch::Padding::xy(12.0f, 0.0f),
                .gap = 6.0f,
                .align_x = etch::AlignX::Center,
                .align_y = etch::AlignY::Center,
                .background = jh ? pal.bg_elevated : pal.bg_surface,
                .border_color = streaming ? pal.accent_border : (jh ? pal.border_strong : pal.border_default),
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusPill),
                .shadow_color = etch::Color::rgba8(0, 0, 0, pal.dark ? 96 : 36),
                .shadow_blur = 14.0f,
                .shadow_offset = {0.0f, 4.0f},
                .attach_to = etch::AttachTo::Parent,
                .anchor_element = etch::Anchor::BottomCenter,
                .anchor_parent = etch::Anchor::BottomCenter,
                .floating_offset = {0.0f, -14.0f},
                .z_index = 20,
                .continuous_corners = true,
                .press_scale = 0.96f,
                .transition = kMotionSnap,
            }) {
                etch::icon({
                    .icon_id = app.icons.arrow_down,
                    .size_px = 12.5f,
                    .color = streaming ? pal.accent : (jh ? pal.text_primary : pal.text_secondary),
                });
                etch::text(app.ts("chat.jump_to_bottom", "Jump to latest"), {
                    .color = jh ? pal.text_primary : pal.text_secondary,
                    .font_size_px = ts.ui_sm,
                    .font_id = app.fonts.medium,
                    .wrap = false,
                });
            }
            if (jh) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(jump_id)) {
                etch::set_scroll_stuck_to_bottom(scroll_id, true);
            }
        }
    }
}

} // namespace ompe
