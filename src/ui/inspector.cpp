#include "ui/app.h"
#include "ui/diff_view.h"
#include "core/platform.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace ompe {
namespace {

constexpr float kTreeCardW   = 228.0f;
constexpr float kTreeCardH   = 88.0f;
constexpr float kTreeLaneDX  = 256.0f;
constexpr float kTreeDepthDY = 118.0f;

struct FlatTreeNode {
    const SessionTreeNode* node = nullptr;
    int parent_flat_idx = -1;
    etch::Vec2 pos{};
    std::string nearest_user_id;
    bool on_active_path = false;
};

bool collect_active_path(const SessionTreeNode& node, const std::string& leaf_id,
                         std::unordered_set<std::string>& out_path) {
    if (leaf_id.empty()) return false;
    if (node.id == leaf_id) {
        out_path.insert(node.id);
        return true;
    }
    for (const auto& ch : node.children) {
        if (collect_active_path(ch, leaf_id, out_path)) {
            out_path.insert(node.id);
            return true;
        }
    }
    return false;
}

bool tree_has_any_turns(const SessionTreeNode& node) {
    if (node.is_conversation_turn()) return true;
    for (const auto& ch : node.children) {
        if (tree_has_any_turns(ch)) return true;
    }
    return false;
}

void layout_tree_recursive(const SessionTreeNode& node, int parent_idx, int depth,
                           int& lane_counter, bool turns_only,
                           const std::string& leaf_id,
                           const std::unordered_set<std::string>& active_path,
                           const std::string& inherited_user_id,
                           std::vector<FlatTreeNode>& out) {
    const std::string cur_user_id = (node.type == "message" && node.role == "user")
        ? node.id : inherited_user_id;
    const bool keep = !turns_only || node.is_conversation_turn() || node.id == leaf_id || !node.label.empty();

    int next_parent = parent_idx;
    int next_depth = depth;
    if (keep) {
        const int my_idx = static_cast<int>(out.size());
        const int my_lane = lane_counter;
        out.push_back(FlatTreeNode{
            .node = &node,
            .parent_flat_idx = parent_idx,
            .pos = {20.0f + static_cast<float>(my_lane) * kTreeLaneDX,
                    20.0f + static_cast<float>(depth) * kTreeDepthDY},
            .nearest_user_id = cur_user_id,
            .on_active_path = active_path.count(node.id) > 0,
        });
        next_parent = my_idx;
        next_depth = depth + 1;
    }

    size_t placed_children = 0;
    for (size_t i = 0; i < node.children.size(); ++i) {
        const size_t before_sz = out.size();
        if (placed_children > 0) ++lane_counter;
        layout_tree_recursive(node.children[i], next_parent, next_depth, lane_counter,
                              turns_only, leaf_id, active_path, cur_user_id, out);
        if (out.size() > before_sz) {
            ++placed_children;
        } else if (placed_children > 0) {
            --lane_counter;
        }
    }
}

std::string basename_only(const std::string& p) {
    const size_t pos = p.find_last_of("/\\");
    return (pos == std::string::npos) ? p : p.substr(pos + 1);
}

const char* tree_role_label(const SessionTreeNode& tn, const I18nCatalog& i18n) noexcept {
    if (tn.role == "user")       return i18n.get("tree.role.user", "User");
    if (tn.role == "assistant")  return i18n.get("tree.role.assistant", "Assistant");
    if (tn.role == "toolResult") return i18n.get("tree.role.tool", "Tool");
    if (tn.role == "compaction") return i18n.get("tree.role.compaction", "Compact");
    if (tn.role == "branch")     return i18n.get("tree.role.branch", "Branch");
    if (tn.role == "model")      return i18n.get("tree.role.model", "Model");
    if (tn.role == "thinking")   return i18n.get("tree.role.thinking", "Effort");
    return i18n.get("tree.role.meta", "Meta");
}

uint16_t tree_role_icon(const SessionTreeNode& tn, const AppIcons& ico) noexcept {
    if (tn.role == "user")       return ico.user_circle;
    if (tn.role == "assistant")  return ico.bot_node;
    if (tn.role == "toolResult") return ico.wrench_node;
    if (tn.role == "compaction") return ico.compress;
    if (tn.role == "branch")     return ico.git_fork;
    if (tn.role == "model")      return ico.cpu;
    if (tn.role == "thinking")   return ico.brain_circuit;
    return ico.info;
}

etch::Color tree_role_color(const SessionTreeNode& tn, const Palette& pal) noexcept {
    if (tn.role == "user")       return pal.accent;
    if (tn.role == "assistant")  return pal.secondary;
    if (tn.role == "toolResult") return pal.warning;
    if (tn.role == "compaction" || tn.role == "branch") return pal.success;
    return pal.text_muted;
}

// ============================================================================
// Tab 0: Workbench Changes, File Diff & Expanded Diagram Preview
// ============================================================================
void build_changes_tab(App& app, const Palette& pal) {
    const TypeScale ts = app.type();
    // Mode 1: Expanded Mermaid Architecture Diagram Preview
    if (!app.focused_workbench_diagram.empty()) {
        ETCH_BOX({
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::grow(),
            .padding = etch::Padding::all(12.0f),
            .gap = 10.0f,
            .shrink_height = true,
        }) {
            ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 8.0f, .align_y = etch::AlignY::Center}) {
                if (etch::button({.id = etch::id("wb_diag_back"), .label = app.ts("workbench.files_back", "← Files"),
                                  .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(24.0f)})) {
                    app.focused_workbench_diagram.clear();
                }
                etch::text(app.ts("workbench.diagram_preview", "Diagram Preview"), {
                    .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.semibold, .wrap = false
                });
                etch::box({.width = etch::Sizing::grow()});
                if (etch::button({.id = etch::id("wb_diag_copy"), .label = app.ts("common.copy", "Copy"),
                                  .icon_id = app.icons.copy,
                                  .variant = etch::ButtonVariant::Ghost, .height = etch::Sizing::fixed(24.0f)})) {
                    app.copy_with_feedback("wb_diag", app.focused_workbench_diagram, etch::pointer_pos());
                }
            }

            const uint32_t diag_box_id = etch::id("wb_diag_box");
            const etch::Rect db = etch::element_bounds(diag_box_id);
            const float avail_w = (db.w > 80.0f) ? (db.w - 24.0f) : 340.0f;
            ETCH_BOX({
                .id = diag_box_id,
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::grow(),
                .padding = etch::Padding::all(12.0f),
                .align_x = etch::AlignX::Center,
                .align_y = etch::AlignY::Center,
                .background = pal.bg_canvas,
                .border_color = pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusMd),
                .clip_children = true,
                .scroll_x = true,
                .scroll_y = true,
                .continuous_corners = true,
            }) {
                inlay::DiagramDesc dd = inlay::diagram_desc_default();
                dd.font_size_px = ts.ui_sm;
                dd.max_width_px = avail_w;
                dd.min_scale = 0.55f;
                dd.has_custom_fonts = true;
                dd.font_id = app.fonts.body;
                dd.bold_font_id = app.fonts.semibold;
                inlay::diagram_view(EtchStr{app.focused_workbench_diagram.c_str(),
                                            static_cast<uint32_t>(app.focused_workbench_diagram.size()), false},
                                    &dd);
            }
        }
        return;
    }

    // Collect session tool executions in chronological conversation order
    std::vector<const ToolExecution*> mutations;
    std::vector<const ToolExecution*> shells;
    std::vector<const ToolExecution*> reads;
    std::unordered_set<std::string> seen;

    for (const auto& m : app.client.messages()) {
        for (const auto& b : m.blocks) {
            if (b.kind != ContentBlockKind::ToolCall) continue;
            if (!seen.insert(b.tool_call_id).second) continue;
            auto it = app.client.tool_executions().find(b.tool_call_id);
            if (it == app.client.tool_executions().end()) continue;
            const ToolExecution& te = it->second;
            if (is_mutation_tool_name(te.tool_name)) {
                mutations.push_back(&te);
            } else if (te.tool_name == "bash" || te.tool_name == "shell") {
                shells.push_back(&te);
            } else if (te.tool_name == "read" || te.tool_name == "read_file" ||
                       te.tool_name == "grep" || te.tool_name == "find" || te.tool_name == "glob") {
                reads.push_back(&te);
            }
        }
    }

    if (mutations.empty() && shells.empty() && reads.empty()) {
        ETCH_BOX({
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::grow(),
            .padding = etch::Padding::all(24.0f),
            .gap = 8.0f,
            .align_x = etch::AlignX::Center,
            .align_y = etch::AlignY::Center,
        }) {
            etch::icon({.icon_id = app.icons.file_diff, .size_px = 24.0f, .color = pal.text_faint});
            etch::text(app.ts("workbench.empty_title", "Workbench is clear"), {
                .color = pal.text_secondary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false
            });
            etch::text(app.ts("workbench.empty_desc",
                              "Edited files, read traces, terminal runs, and diagrams appear here for side-by-side inspection."), {
                .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true, .align_x = etch::AlignX::Center
            });
        }
        return;
    }

    // Auto-select first mutation (or shell/read) if nothing focused yet
    const ToolExecution* active = nullptr;
    if (!app.focused_workbench_tool_id.empty()) {
        auto it = app.client.tool_executions().find(app.focused_workbench_tool_id);
        if (it != app.client.tool_executions().end()) active = &it->second;
    }
    if (!active) {
        if (!mutations.empty()) active = mutations.front();
        else if (!shells.empty()) active = shells.front();
        else if (!reads.empty()) active = reads.front();
        if (active) app.focused_workbench_tool_id = active->tool_call_id;
    }

    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::grow(),
        .padding = etch::Padding::all(10.0f),
        .gap = 8.0f,
        .shrink_height = true,
    }) {
        // Compact artifact selector list (Modified -> Commands -> Explored)
        ETCH_BOX({
            .id = etch::id("wb_artifact_list"),
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fit(0.0f, 168.0f),
            .padding = {.right = 8.0f},
            .gap = 2.0f,
            .clip_children = true,
            .scroll_y = true,
            .scroll_fade_px = 12.0f,
        }) {
            uint32_t item_idx = 0;
            auto emit_section = [&](const char* heading, const std::vector<const ToolExecution*>& items, uint16_t ico, bool is_diff) {
                if (items.empty()) return;
                etch::text(etch::str_cstr(heading), {
                    .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.semibold, .letter_spacing = 0.8f, .wrap = false
                });
                for (const ToolExecution* te : items) {
                    const uint32_t rid = etch::id_index(etch::id("wb_item"), item_idx++);
                    const bool sel = (active && active->tool_call_id == te->tool_call_id);
                    const bool hov = etch::hovered(rid);
                    std::string lbl = te->args_summary.empty() ? te->tool_name : basename_only(te->args_summary);
                    if ((te->tool_name == "grep" || te->tool_name == "search" || te->tool_name == "ripgrep") && !te->args_summary.empty()) {
                        const size_t in_pos = te->args_summary.find(" in ");
                        lbl = "/" + ((in_pos != std::string::npos) ? te->args_summary.substr(0, in_pos) : te->args_summary) + "/";
                    }
                    const auto [a, d] = is_diff ? count_diff_stats(best_diff_text(*te)) : std::pair<int, int>{0, 0};

                    ETCH_BOX({
                        .id = rid,
                        .axis = etch::Axis::Row,
                        .width = etch::Sizing::grow(),
                        .height = etch::Sizing::fixed(ts.pill_h + 1.0f),
                        .padding = etch::Padding::xy(8.0f, 0.0f),
                        .gap = 7.0f,
                        .align_y = etch::AlignY::Center,
                        .background = sel ? (pal.dark ? etch::Color::rgba8(244, 238, 226, 14) : etch::Color::rgba8(28, 25, 21, 12))
                                          : (hov ? pal.hover : etch::Color::transparent()),
                        .border_color = sel ? pal.border_subtle : etch::Color::transparent(),
                        .border_width = sel ? 1.0f : 0.0f,
                        .radius = etch::Corners::all(6.0f),
                        .continuous_corners = true,
                    }) {
                        etch::icon({
                            .icon_id = ico,
                            .size_px = 13.0f,
                            .color = sel ? pal.accent : pal.text_muted,
                        });
                        ETCH_BOX({.width = etch::Sizing::grow(), .shrink_width = true}) {
                            etch::text(etch::str_cstr(lbl.c_str()), {
                                .color = sel ? pal.text_primary : pal.text_secondary,
                                .font_size_px = ts.code - 0.5f,
                                .font_id = app.fonts.mono,
                                .wrap = false,
                                .ellipsis = true,
                            });
                        }
                        if (is_diff && (a > 0 || d > 0)) {
                            ETCH_BOX({.axis = etch::Axis::Row, .gap = 4.0f, .align_y = etch::AlignY::Center}) {
                                if (a > 0) {
                                    char ab[16];
                                    std::snprintf(ab, sizeof(ab), "+%d", a);
                                    etch::text(etch::str_cstr(ab), {
                                        .color = pal.success, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                                    });
                                }
                                if (d > 0) {
                                    char db[16];
                                    std::snprintf(db, sizeof(db), "-%d", d);
                                    etch::text(etch::str_cstr(db), {
                                        .color = pal.danger, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                                    });
                                }
                            }
                        }
                    }
                    if (hov) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(rid)) {
                        app.focused_workbench_tool_id = te->tool_call_id;
                    }
                }
            };

            emit_section(app.t("workbench.section.modified", "MODIFIED FILES"), mutations, app.icons.file_diff, true);
            emit_section(app.t("workbench.section.commands", "COMMANDS"), shells, app.icons.terminal, false);
            emit_section(app.t("workbench.section.explored", "EXPLORED CONTEXT"), reads, app.icons.eye_open, false);
        }

        // Spacious focused inspector surface for the selected file / diff / terminal
        if (active) {
            const std::string& focused_diff = best_diff_text(*active);
            const bool has_structured_diff = looks_like_structured_diff(focused_diff);
            const bool is_diff = is_mutation_tool_name(active->tool_name) || has_structured_diff;
            const bool is_shell = (active->tool_name == "bash" || active->tool_name == "shell");
            const auto [fa, fd] = is_diff ? count_diff_stats(focused_diff) : std::pair<int, int>{0, 0};

            ETCH_BOX({
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::grow(),
                .background = pal.bg_canvas,
                .border_color = pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusMd),
                .clip_children = true,
                .shrink_height = true,
                .continuous_corners = true,
            }) {
                // Header breadcrumb + +/- diff stat + copy
                ETCH_BOX({
                    .axis = etch::Axis::Row,
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::fixed(ts.row_h + 2.0f),
                    .padding = etch::Padding::xy(10.0f, 0.0f),
                    .gap = 7.0f,
                    .align_y = etch::AlignY::Center,
                    .background = pal.bg_surface,
                    .border_color = pal.border_subtle,
                    .border_sides = {.bottom = 1.0f},
                }) {
                    etch::icon({
                        .icon_id = is_diff ? app.icons.file_diff_duotone : (is_shell ? app.icons.terminal_duotone : app.icons.eye_open),
                        .size_px = 14.0f,
                        .color = pal.accent,
                    });
                    ETCH_BOX({.width = etch::Sizing::grow(), .shrink_width = true}) {
                        etch::text(etch::str_cstr(active->args_summary.empty() ? active->tool_name.c_str() : active->args_summary.c_str()), {
                            .color = pal.text_primary, .font_size_px = ts.code - 0.5f, .font_id = app.fonts.mono, .wrap = false, .ellipsis = true
                        });
                    }
                    if (is_diff && (fa > 0 || fd > 0)) {
                        ETCH_BOX({.axis = etch::Axis::Row, .gap = 4.0f, .align_y = etch::AlignY::Center}) {
                            if (fa > 0) {
                                char ab[16];
                                std::snprintf(ab, sizeof(ab), "+%d", fa);
                                etch::text(etch::str_cstr(ab), {
                                    .color = pal.success, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                                });
                            }
                            if (fd > 0) {
                                char db[16];
                                std::snprintf(db, sizeof(db), "-%d", fd);
                                etch::text(etch::str_cstr(db), {
                                    .color = pal.danger, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                                });
                            }
                        }
                    }
                    const uint32_t cp_id = etch::id("wb_copy_focused");
                    const bool cph = etch::hovered(cp_id);
                    const bool copied = (app.copied_key == "wb_focused");
                    ETCH_BOX({
                        .id = cp_id,
                        .width = etch::Sizing::fixed(22.0f),
                        .height = etch::Sizing::fixed(22.0f),
                        .align_x = etch::AlignX::Center,
                        .align_y = etch::AlignY::Center,
                        .background = cph ? pal.hover : etch::Color::transparent(),
                        .radius = etch::Corners::all(5.0f),
                    }) {
                        etch::icon({
                            .icon_id = copied ? app.icons.check : app.icons.copy,
                            .size_px = 11.5f,
                            .color = copied ? pal.success : (cph ? pal.text_primary : pal.text_muted),
                        });
                    }
                    etch::set_tooltip(cp_id, app.ts("workbench.copy_content", "Copy content"));
                    if (cph) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(cp_id)) {
                        const etch::Rect r = etch::element_bounds(cp_id);
                        app.copy_with_feedback("wb_focused", is_diff ? focused_diff : active->output_text,
                                               {r.x + r.w * 0.5f, r.y + r.h * 0.5f});
                    }
                }

                // Body preview (edge-to-edge full-bleed diff rows when showing structured diff!)
                ETCH_BOX({
                    .id = etch::id("wb_preview_scroll"),
                    .axis = etch::Axis::Col,
                    .width = etch::Sizing::grow(),
                    .height = etch::Sizing::grow(),
                    .padding = has_structured_diff ? etch::Padding::all(0.0f) : etch::Padding::all(10.0f),
                    .gap = has_structured_diff ? 0.0f : 2.0f,
                    .background = (is_shell && !pal.dark) ? etch::Color::rgb_hex(0x161619) : pal.bg_code,
                    .clip_children = true,
                    .scroll_y = true,
                    .shrink_height = true,
                    .scroll_fade_px = 14.0f,
                }) {
                    if (active->terminal) {
                        const uint32_t vis_rows = std::clamp(active->terminal->used_rows(), 4u, 24u);
                        const etch::Color tbg = pal.dark ? pal.bg_code : etch::Color::rgb_hex(0x161619);
                        const etch::Color tfg = pal.dark ? pal.text_body : etch::Color::rgb_hex(0xE8E3DA);
                        etch::terminal_grid({
                            .id = etch::id("wb_term_grid"),
                            .cells = active->terminal->cells(),
                            .columns = active->terminal->columns(),
                            .rows = vis_rows,
                            .first_row = active->terminal->first_row(),
                            .cell_width_px = ts.term_cell_w * 0.96f,
                            .line_height_px = ts.term_line_h,
                            .font_size_px = ts.code - 0.5f,
                            .font_id = app.fonts.mono,
                            .foreground = tfg,
                            .background = tbg,
                            .cursor_column = active->running ? active->terminal->cursor_col() : -1,
                            .cursor_row = active->running ? active->terminal->cursor_row() : -1,
                            .cursor_color = pal.accent,
                        });
                    } else if (has_structured_diff) {
                        render_structured_diff(focused_diff, pal, app.fonts, 260, ts.code - 0.5f, true);
                    } else if (!focused_diff.empty()) {
                        etch::text(etch::str_cstr(focused_diff.c_str()), {
                            .color = pal.text_secondary, .font_size_px = ts.code - 0.5f, .font_id = app.fonts.mono, .wrap = true, .selectable = true
                        });
                    } else {
                        etch::text(app.ts("workbench.no_output", "No output captured for this step."), {
                            .color = pal.text_faint, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true
                        });
                    }
                }
            }
        }
    }
}

void build_agents_tab(App& app, const Palette& pal) {
    const TypeScale ts = app.type();
    const auto& agents = app.client.subagents();
    if (agents.empty()) {
        ETCH_BOX({
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::grow(),
            .padding = etch::Padding::all(24.0f),
            .gap = 8.0f,
            .align_x = etch::AlignX::Center,
            .align_y = etch::AlignY::Center,
        }) {
            etch::icon({.icon_id = app.icons.agents, .size_px = 24.0f, .color = pal.text_faint});
            etch::text(app.ts("agents.empty_title", "No active subagents"), {
                .color = pal.text_secondary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false
            });
            etch::text(app.ts("agents.empty_desc", "Spawned background workers appear here in real time."), {
                .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true, .align_x = etch::AlignX::Center
            });
        }
        return;
    }

    ETCH_BOX({
        .id = etch::id("ins_agents_scroll"),
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::grow(),
        .padding = etch::Padding::all(12.0f),
        .gap = 8.0f,
        .clip_children = true,
        .scroll_y = true,
        .shrink_height = true,
        .scroll_fade_px = 18.0f,
    }) {
        for (size_t i = 0; i < agents.size(); ++i) {
            const SubagentInfo& sa = agents[i];
            const bool running = (sa.status == "running" || sa.status == "active" || sa.status == "pending");
            const bool selected = (app.selected_subagent_id == sa.id);
            const uint32_t cid = etch::id_index(etch::id("agent_card"), static_cast<uint32_t>(i));
            const etch::Color st_col = running ? pal.accent :
                                       (sa.status == "completed" ? pal.success : pal.danger);

            ETCH_BOX({
                .id = cid,
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .padding = etch::Padding::all(12.0f),
                .gap = 7.0f,
                .background = selected ? pal.bg_elevated : pal.bg_surface,
                .border_color = selected ? pal.accent_border : pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusMd),
                .continuous_corners = true,
            }) {
                ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 7.0f, .align_y = etch::AlignY::Center}) {
                    etch::icon({
                        .icon_id = running ? app.icons.loader_arc : (sa.status == "completed" ? app.icons.badge_check : app.icons.x_circle),
                        .size_px = 13.0f,
                        .color = st_col,
                        .rotation_rad = running ? (app.elapsed_s * 5.2f) : 0.0f,
                    });
                    ETCH_BOX({.width = etch::Sizing::grow(), .shrink_width = true}) {
                        etch::text(etch::str_cstr(sa.id.c_str()), {
                            .color = pal.text_primary, .font_size_px = ts.ui, .font_id = app.fonts.semibold, .wrap = false, .ellipsis = true
                        });
                    }
                    const char* st_lbl = running ? app.t("status.running", "running")
                                                 : (sa.status == "completed" ? app.t("status.completed", "completed") : sa.status.c_str());
                    etch::text(etch::str_cstr(st_lbl), {
                        .color = st_col, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                    });
                    if (sa.can_cancel) {
                        const uint32_t kill_id = etch::id_index(etch::id("agent_kill"), static_cast<uint32_t>(i));
                        const bool kh = etch::hovered(kill_id);
                        ETCH_BOX({
                            .id = kill_id,
                            .width = etch::Sizing::fixed(20.0f),
                            .height = etch::Sizing::fixed(20.0f),
                            .align_x = etch::AlignX::Center,
                            .align_y = etch::AlignY::Center,
                            .background = kh ? pal.danger_soft : etch::Color::transparent(),
                            .radius = etch::Corners::all(5.0f),
                        }) {
                            etch::icon({.icon_id = app.icons.stop, .size_px = 10.5f, .color = kh ? pal.danger : pal.text_muted});
                        }
                        if (kh) etch::set_cursor(etch::Cursor::Hand);
                        etch::set_tooltip(kill_id, app.ts("agents.stop_tooltip", "Stop subagent"));
                        if (etch::clicked(kill_id)) {
                            app.particles.spawn_dissolve_rect(etch::element_bounds(cid), pal.bg_elevated, pal.danger, DissolveStyle::Ember, 56);
                            if (!app.showcase_mode) app.client.cancel_subagent(sa.id);
                        }
                    }
                }

                const std::string& desc = !sa.description.empty() ? sa.description : sa.task;
                if (!desc.empty()) {
                    etch::text(etch::str_cstr(desc.c_str()), {
                        .color = pal.text_secondary, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true, .max_lines = 2
                    });
                }

                if (!sa.current_tool.empty()) {
                    const std::string tl = sa.current_tool + (sa.current_tool_args.empty() ? "" : (" · " + sa.current_tool_args));
                    ETCH_BOX({
                        .axis = etch::Axis::Row,
                        .width = etch::Sizing::grow(),
                        .padding = etch::Padding::xy(8.0f, 5.0f),
                        .gap = 6.0f,
                        .align_y = etch::AlignY::Center,
                        .background = pal.bg_code,
                        .border_color = pal.border_subtle,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(6.0f),
                    }) {
                        etch::icon({.icon_id = app.icons.wrench_node, .size_px = 11.5f, .color = pal.accent});
                        ETCH_BOX({.width = etch::Sizing::grow(), .shrink_width = true}) {
                            etch::text(etch::str_cstr(tl.c_str()), {
                                .color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false, .ellipsis = true
                            });
                        }
                    }
                }

                const std::string meta = app.tf("agents.stats", "{tokens} · {tools} tools · {cost}", {
                    {"tokens", format_compact_tokens(sa.tokens)},
                    {"tools", std::to_string(sa.tool_count)},
                    {"cost", format_usd(sa.cost_usd)},
                });
                etch::text(etch::str_cstr(meta.c_str()), {
                    .color = pal.text_faint, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
                });

                if (selected && sa.can_steer) {
                    auto sr = etch::text_input({
                        .id = etch::id("agent_steer_input"),
                        .buffer = app.subagent_steer_buf,
                        .capacity = sizeof(app.subagent_steer_buf),
                        .placeholder = app.ts("agents.steer_placeholder", "Steer this subagent (Enter)..."),
                        .width = etch::Sizing::grow(),
                        .height = etch::Sizing::fixed(ts.pill_h + 2.0f),
                        .padding = etch::Padding::xy(8.0f, 4.0f),
                        .font_size_px = ts.ui_sm,
                        .background = pal.bg_input,
                        .border_color = pal.border_default,
                        .focus_border_color = pal.accent_border,
                        .radius = etch::Corners::all(6.0f),
                        .submit_on_enter = true,
                        .focus_glow_blur = 0.0f,
                    });
                    if (sr.submitted && app.subagent_steer_buf[0] != '\0') {
                        if (!app.showcase_mode) app.client.steer_subagent(sa.id, app.subagent_steer_buf);
                        app.client.push_toast(app.tf("toast.steered_agent", "Steered {id}", {{"id", sa.id}}), "info", 2200);
                        app.subagent_steer_buf[0] = '\0';
                    }
                }
            }
            if (etch::hovered(cid)) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(cid)) {
                app.selected_subagent_id = (selected ? "" : sa.id);
                if (!app.selected_subagent_id.empty() && !app.showcase_mode) {
                    app.client.fetch_subagent_messages(app.selected_subagent_id);
                }
            }
        }
    }
}

// ============================================================================
// Tab 2: Architectural Session Branch Tree (DAG Canvas + Filter + Inspector)
// ============================================================================
void build_tree_tab(App& app, const Palette& pal) {
    const TypeScale ts = app.type();
    const auto& roots = app.client.session_tree();

    // Compute full active spine from root -> leafId
    std::unordered_set<std::string> active_path;
    bool has_turns = false;
    for (const auto& root : roots) {
        collect_active_path(root, app.client.tree_leaf_id(), active_path);
        if (tree_has_any_turns(root)) has_turns = true;
    }

    // Fall back to showing all entries automatically when a fresh session only has config entries
    const bool eff_turns_only = app.tree_turns_only && has_turns;
    std::vector<FlatTreeNode> flat;
    int lane = 0;
    for (const auto& root : roots) {
        const size_t before = flat.size();
        layout_tree_recursive(root, -1, 0, lane, eff_turns_only,
                              app.client.tree_leaf_id(), active_path, "", flat);
        if (flat.size() > before) ++lane;
    }
    const int branch_count = std::max(1, lane);

    if (flat.empty()) {
        ETCH_BOX({
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::grow(),
            .padding = etch::Padding::all(24.0f),
            .gap = 10.0f,
            .align_x = etch::AlignX::Center,
            .align_y = etch::AlignY::Center,
        }) {
            etch::icon({.icon_id = app.icons.git_graph, .size_px = 26.0f, .color = pal.text_faint});
            etch::text(app.ts("tree.empty_title", "No conversation tree yet"), {
                .color = pal.text_secondary, .font_size_px = ts.ui, .font_id = app.fonts.medium, .wrap = false
            });
            etch::text(app.ts("tree.empty_desc",
                              "Send a prompt or switch threads to explore and fork conversation branches."), {
                .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true, .align_x = etch::AlignX::Center
            });
            if (!app.showcase_mode &&
                etch::button({.id = etch::id("tree_empty_refresh"), .label = app.ts("tree.refresh_btn", "Refresh Tree"),
                              .icon_id = app.icons.refresh, .variant = etch::ButtonVariant::Default,
                              .height = etch::Sizing::fixed(ts.pill_h)})) {
                app.client.refresh_tree();
            }
        }
        return;
    }

    // Resolve currently focused node (defaults to HEAD leaf so inspector details are immediately helpful)
    const FlatTreeNode* selected_fn = nullptr;
    if (!app.selected_tree_node_id.empty()) {
        for (const auto& fn : flat) {
            if (fn.node->id == app.selected_tree_node_id) { selected_fn = &fn; break; }
        }
    }
    if (!selected_fn) {
        for (const auto& fn : flat) {
            if (fn.node->id == app.client.tree_leaf_id()) { selected_fn = &fn; break; }
        }
        if (!selected_fn) selected_fn = &flat.back();
        app.selected_tree_node_id = selected_fn->node->id;
    }

    const etch::Color grid_min = pal.dark ? etch::Color::rgba8(244, 238, 226, 6) : etch::Color::rgba8(28, 25, 21, 7);
    const etch::Color grid_maj = pal.dark ? etch::Color::rgba8(244, 238, 226, 12) : etch::Color::rgba8(28, 25, 21, 13);
    const uint32_t canvas_id = etch::id("session_tree_canvas");

    if (app.tree_reset_view_req) {
        etch::set_canvas_view(canvas_id, {14.0f, 12.0f}, 0.88f);
        app.tree_reset_view_req = false;
    }

    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::grow(),
        .padding = etch::Padding::all(10.0f),
        .gap = 8.0f,
        .shrink_height = true,
    }) {
        // 1. Compact Tree Toolbar (stats + Turns/All filter + Fit View + Refresh)
        ETCH_BOX({
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .gap = 6.0f,
            .align_y = etch::AlignY::Center,
        }) {
            const std::string stats_str = (branch_count == 1)
                ? app.tf("tree.stats_one", "{nodes} nodes · {branches} branch",
                         {{"nodes", std::to_string(flat.size())}, {"branches", std::to_string(branch_count)}})
                : app.tf("tree.stats_other", "{nodes} nodes · {branches} branches",
                         {{"nodes", std::to_string(flat.size())}, {"branches", std::to_string(branch_count)}});
            etch::text(etch::str_cstr(stats_str.c_str()), {
                .color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.mono_tnum, .wrap = false
            });
            etch::box({.width = etch::Sizing::grow()});

            // Turns-only vs All entries filter chip
            const uint32_t flt_id = etch::id("tree_filter_btn");
            const bool flt_hov = etch::hovered(flt_id);
            ETCH_BOX({
                .id = flt_id,
                .axis = etch::Axis::Row,
                .height = etch::Sizing::fixed(24.0f),
                .padding = etch::Padding::xy(8.0f, 0.0f),
                .gap = 5.0f,
                .align_y = etch::AlignY::Center,
                .background = flt_hov ? pal.hover : pal.bg_surface,
                .border_color = eff_turns_only ? pal.accent_border : pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusPill),
                .continuous_corners = true,
            }) {
                etch::icon({.icon_id = app.icons.filter_lines, .size_px = 11.5f,
                            .color = eff_turns_only ? pal.accent : pal.text_muted});
                etch::text(eff_turns_only ? app.ts("tree.filter_turns", "Turns") : app.ts("tree.filter_all", "All"), {
                    .color = eff_turns_only ? pal.text_primary : pal.text_muted,
                    .font_size_px = ts.meta, .font_id = app.fonts.medium, .wrap = false
                });
            }
            etch::set_tooltip(flt_id, app.ts("tree.filter_tooltip", "Toggle conversation turns vs all internal entries"));
            if (flt_hov) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(flt_id)) app.tree_turns_only = !app.tree_turns_only;

            // Fit / reset canvas button
            const uint32_t fit_id = etch::id("tree_fit_btn");
            const bool fit_hov = etch::hovered(fit_id);
            ETCH_BOX({
                .id = fit_id,
                .width = etch::Sizing::fixed(24.0f),
                .height = etch::Sizing::fixed(24.0f),
                .align_x = etch::AlignX::Center,
                .align_y = etch::AlignY::Center,
                .background = fit_hov ? pal.hover : pal.bg_surface,
                .border_color = pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(6.0f),
            }) {
                etch::icon({.icon_id = app.icons.focus_fit, .size_px = 12.0f, .color = fit_hov ? pal.accent : pal.text_muted});
            }
            etch::set_tooltip(fit_id, app.ts("tree.reset_view", "Reset view"));
            if (fit_hov) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(fit_id)) app.tree_reset_view_req = true;

            // Refresh tree button
            const uint32_t ref_id = etch::id("tree_ref_btn");
            const bool ref_hov = etch::hovered(ref_id);
            ETCH_BOX({
                .id = ref_id,
                .width = etch::Sizing::fixed(24.0f),
                .height = etch::Sizing::fixed(24.0f),
                .align_x = etch::AlignX::Center,
                .align_y = etch::AlignY::Center,
                .background = ref_hov ? pal.hover : pal.bg_surface,
                .border_color = pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(6.0f),
            }) {
                etch::icon({.icon_id = app.icons.refresh, .size_px = 12.0f, .color = ref_hov ? pal.accent : pal.text_muted});
            }
            etch::set_tooltip(ref_id, app.ts("tree.refresh_tooltip", "Refresh tree"));
            if (ref_hov) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(ref_id) && !app.showcase_mode) app.client.refresh_tree();
        }

        // 2. Interactive Pannable / Zoomable DAG Canvas
        etch::begin_canvas({
            .id = canvas_id,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::grow(),
            .default_pan = {14.0f, 12.0f},
            .default_zoom = 0.88f,
            .min_zoom = 0.35f,
            .max_zoom = 2.0f,
            .grid_step_world = 30.0f,
            .background = pal.bg_code,
            .minor_grid_color = grid_min,
            .major_grid_color = grid_maj,
            .border_color = pal.border_subtle,
            .border_width = 1.0f,
            .radius = etch::Corners::all(kRadiusMd),
        });

        // Emit cubic Bezier wires: entire active ancestor spine glows cleanly!
        for (size_t i = 0; i < flat.size(); ++i) {
            if (flat[i].parent_flat_idx >= 0) {
                const auto& par = flat[static_cast<size_t>(flat[i].parent_flat_idx)];
                const bool active_edge = flat[i].on_active_path && par.on_active_path;
                etch::canvas_wire({
                    .from_world = {par.pos.x + kTreeCardW * 0.5f, par.pos.y + kTreeCardH},
                    .to_world   = {flat[i].pos.x + kTreeCardW * 0.5f, flat[i].pos.y},
                    .color_start = active_edge ? pal.accent : pal.border_strong,
                    .color_end = active_edge ? pal.secondary : pal.border_strong,
                    .glow_color = active_edge ? pal.accent_soft : etch::Color::transparent(),
                    .thickness_world = active_edge ? 2.2f : 1.35f,
                    .glow_world = active_edge ? 4.0f : 0.0f,
                    .tangent_bias = 0.48f,
                    .axis = etch::Axis::Col,
                });
            }
        }

        // Emit sculpted architectural node cards (zero text overflow, 2-line wrapped preview!)
        for (size_t i = 0; i < flat.size(); ++i) {
            const SessionTreeNode& tn = *flat[i].node;
            const bool is_leaf = (tn.id == app.client.tree_leaf_id());
            const bool is_sel = (tn.id == app.selected_tree_node_id);
            const bool on_spine = flat[i].on_active_path;
            const uint32_t nid = etch::id_index(etch::id("tree_node"), static_cast<uint32_t>(i));
            const etch::Color rcol = tree_role_color(tn, pal);

            const etch::Color card_bg = is_sel
                ? pal.bg_elevated
                : (on_spine ? pal.bg_surface : pal.bg_canvas);
            const etch::Color card_border = is_sel
                ? pal.accent
                : (is_leaf ? pal.accent_border : (on_spine ? pal.border_default : pal.border_subtle));

            const auto nr = etch::begin_canvas_node({
                .id = nid,
                .pos_value = flat[i].pos,
                .width_world = etch::Sizing::fixed(kTreeCardW),
                .height_world = etch::Sizing::fixed(kTreeCardH),
                .padding_world = etch::Padding::xy(11.0f, 9.0f),
                .gap_world = 6.0f,
                .background = card_bg,
                .border_color = card_border,
                .border_width_world = (is_sel || is_leaf) ? 1.5f : 1.0f,
                .radius_world = etch::Corners::all(11.0f),
                .shadow_color = pal.shadow,
                .shadow_blur_world = is_sel ? 16.0f : 9.0f,
                .draggable = false,
            });

            // Header Row: semantic icon badge + localized role + HEAD/label pill (shrink-safe!)
            ETCH_BOX({
                .axis = etch::Axis::Row,
                .width = etch::Sizing::grow(),
                .gap = 6.0f,
                .align_y = etch::AlignY::Center,
                .shrink_width = true,
            }) {
                ETCH_BOX({
                    .width = etch::Sizing::fixed(20.0f),
                    .height = etch::Sizing::fixed(20.0f),
                    .align_x = etch::AlignX::Center,
                    .align_y = etch::AlignY::Center,
                    .background = etch::Color::rgba8(rcol.r, rcol.g, rcol.b, pal.dark ? 32 : 24),
                    .radius = etch::Corners::all(6.0f),
                }) {
                    etch::icon({
                        .icon_id = tree_role_icon(tn, app.icons),
                        .size_px = 11.5f,
                        .color = rcol,
                    });
                }

                ETCH_BOX({.width = etch::Sizing::grow(), .shrink_width = true}) {
                    etch::text(etch::str_cstr(tree_role_label(tn, app.i18n)), {
                        .color = on_spine ? pal.text_primary : pal.text_secondary,
                        .font_size_px = ts.meta,
                        .font_id = app.fonts.semibold,
                        .wrap = false,
                        .ellipsis = true,
                    });
                }

                if (is_leaf) {
                    ETCH_BOX({
                        .padding = etch::Padding::xy(6.0f, 1.5f),
                        .background = pal.accent_soft,
                        .border_color = pal.accent_border,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(kRadiusPill),
                    }) {
                        etch::text(app.ts("tree.head_badge", "HEAD"), {
                            .color = pal.accent,
                            .font_size_px = ts.micro,
                            .font_id = app.fonts.mono_tnum,
                            .wrap = false,
                        });
                    }
                } else if (!tn.label.empty()) {
                    ETCH_BOX({
                        .width = etch::Sizing::fit(0.0f, 84.0f),
                        .padding = etch::Padding::xy(6.0f, 1.5f),
                        .background = pal.bg_input,
                        .border_color = pal.border_subtle,
                        .border_width = 1.0f,
                        .radius = etch::Corners::all(kRadiusPill),
                        .shrink_width = true,
                    }) {
                        etch::text(etch::str_cstr(tn.label.c_str()), {
                            .color = pal.text_muted,
                            .font_size_px = ts.micro,
                            .font_id = app.fonts.mono,
                            .wrap = false,
                            .ellipsis = true,
                        });
                    }
                }
            }

            // Body: crisp 2-line wrapped preview (never single-line clipped gibberish!)
            ETCH_BOX({
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::grow(),
                .clip_children = true,
                .shrink_width = true,
                .shrink_height = true,
            }) {
                const std::string& pv = tn.preview.empty() ? tn.type : tn.preview;
                etch::text(etch::str_cstr(pv.c_str()), {
                    .color = on_spine ? pal.text_body : pal.text_muted,
                    .font_size_px = ts.meta,
                    .font_id = app.fonts.body,
                    .wrap = true,
                    .ellipsis = true,
                    .max_lines = 2,
                });
            }

            etch::end_canvas_node();

            if (nr.clicked) {
                app.selected_tree_node_id = tn.id;
            }
        }
        etch::end_canvas();

        // 3. Selected Node Detail & Actions Card
        if (selected_fn && selected_fn->node) {
            const SessionTreeNode& sel = *selected_fn->node;
            const bool is_head = (sel.id == app.client.tree_leaf_id());
            const std::string short_id = sel.id.size() > 8 ? sel.id.substr(0, 8) : sel.id;
            const etch::Color rcol = tree_role_color(sel, pal);

            ETCH_BOX({
                .axis = etch::Axis::Col,
                .width = etch::Sizing::grow(),
                .padding = etch::Padding::all(10.0f),
                .gap = 8.0f,
                .background = pal.bg_surface,
                .border_color = is_head ? pal.accent_border : pal.border_subtle,
                .border_width = 1.0f,
                .radius = etch::Corners::all(kRadiusMd),
                .continuous_corners = true,
            }) {
                ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 6.0f, .align_y = etch::AlignY::Center}) {
                    etch::icon({.icon_id = tree_role_icon(sel, app.icons), .size_px = 13.0f, .color = rcol});
                    etch::text(etch::str_cstr(tree_role_label(sel, app.i18n)), {
                        .color = pal.text_primary, .font_size_px = ts.ui_sm, .font_id = app.fonts.semibold, .wrap = false
                    });
                    etch::text(etch::str_cstr(("#" + short_id).c_str()), {
                        .color = pal.text_faint, .font_size_px = ts.micro, .font_id = app.fonts.mono_tnum, .wrap = false
                    });
                    etch::box({.width = etch::Sizing::grow()});
                    if (is_head) {
                        etch::text(app.ts("tree.active_head", "Active HEAD"), {
                            .color = pal.accent, .font_size_px = ts.micro, .font_id = app.fonts.medium, .wrap = false
                        });
                    } else if (!sel.label.empty()) {
                        etch::text(etch::str_cstr(sel.label.c_str()), {
                            .color = pal.text_muted, .font_size_px = ts.micro, .font_id = app.fonts.mono, .wrap = false
                        });
                    }
                }

                etch::text(etch::str_cstr(sel.preview.empty() ? sel.type.c_str() : sel.preview.c_str()), {
                    .color = pal.text_secondary,
                    .font_size_px = ts.ui_sm,
                    .font_id = app.fonts.body,
                    .wrap = true,
                    .ellipsis = true,
                    .max_lines = 3,
                });

                ETCH_BOX({.axis = etch::Axis::Row, .width = etch::Sizing::grow(), .gap = 6.0f}) {
                    if (etch::button({
                            .id = etch::id("tree_switch"),
                            .label = is_head ? app.ts("tree.current_head", "Current HEAD") : app.ts("tree.switch_here", "Switch Here"),
                            .icon_id = app.icons.arrow_switch,
                            .variant = is_head ? etch::ButtonVariant::Ghost : etch::ButtonVariant::Primary,
                            .width = etch::Sizing::grow(),
                            .height = etch::Sizing::fixed(ts.pill_h),
                            .disabled = is_head,
                        })) {
                        if (!app.showcase_mode) {
                            app.stick_chat_bottom = true;
                            app.client.navigate_tree(sel.id, false);
                        } else {
                            app.client.push_toast(app.t("toast.tree_switched_demo", "Switched active branch (Showcase)"), "info", 2200);
                        }
                    }

                    if (!is_head && etch::button({
                            .id = etch::id("tree_summarize"),
                            .label = app.ts("tree.summarize", "Summarize"),
                            .icon_id = app.icons.compress,
                            .variant = etch::ButtonVariant::Default,
                            .height = etch::Sizing::fixed(ts.pill_h),
                        })) {
                        if (!app.showcase_mode) {
                            app.stick_chat_bottom = true;
                            app.client.navigate_tree(sel.id, true);
                        } else {
                            app.client.push_toast(app.t("toast.tree_summary_demo", "Switched with branch summary (Showcase)"), "info", 2200);
                        }
                    }

                    const std::string& fork_target = selected_fn->nearest_user_id;
                    if (etch::button({
                            .id = etch::id("tree_fork"),
                            .label = app.ts("tree.fork", "Fork"),
                            .icon_id = app.icons.git_fork,
                            .variant = etch::ButtonVariant::Default,
                            .height = etch::Sizing::fixed(ts.pill_h),
                            .disabled = fork_target.empty(),
                        })) {
                        if (!app.showcase_mode) {
                            app.client.fork_session(fork_target);
                        } else {
                            app.client.push_toast(app.t("toast.tree_fork_demo", "Forked new thread from node (Showcase)"), "success", 2400);
                        }
                    }
                }
            }
        }
    }
}

void build_btw_tab(App& app, const Palette& pal) {
    const TypeScale ts = app.type();
    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::grow(),
        .padding = etch::Padding::all(12.0f),
        .gap = 10.0f,
        .shrink_height = true,
    }) {
        ETCH_BOX({
            .id = etch::id("btw_scroll"),
            .axis = etch::Axis::Col,
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::grow(),
            .gap = 10.0f,
            .clip_children = true,
            .scroll_y = true,
            .shrink_height = true,
            .scroll_fade_px = 16.0f,
            .stick_to_bottom = true,
        }) {
            if (app.client.btw_records().empty()) {
                etch::text(app.ts("btw.empty_desc",
                                  "Ask side questions over the current thread without adding them to main history."), {
                    .color = pal.text_muted, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true
                });
            }
            for (const auto& rec : app.client.btw_records()) {
                ETCH_BOX({
                    .axis = etch::Axis::Col,
                    .width = etch::Sizing::grow(),
                    .padding = etch::Padding::all(11.0f),
                    .gap = 6.0f,
                    .background = pal.bg_surface,
                    .border_color = pal.border_subtle,
                    .border_width = 1.0f,
                    .radius = etch::Corners::all(kRadiusMd),
                    .continuous_corners = true,
                }) {
                    etch::text(etch::str_cstr(rec.question.c_str()), {
                        .color = pal.accent, .font_size_px = ts.ui_sm, .font_id = app.fonts.medium, .wrap = true
                    });
                    if (!rec.answer.empty()) {
                        etch::text(etch::str_cstr(rec.answer.c_str()), {
                            .color = pal.text_body, .font_size_px = ts.ui_sm, .font_id = app.fonts.body, .wrap = true, .selectable = true
                        });
                    } else if (rec.status == "running") {
                        ETCH_BOX({.axis = etch::Axis::Row, .gap = 6.0f, .align_y = etch::AlignY::Center}) {
                            etch::icon({.icon_id = app.icons.loader_arc, .size_px = 11.5f, .color = pal.accent, .rotation_rad = app.elapsed_s * 5.2f});
                            etch::text(app.ts("btw.answering", "Answering..."), {
                                .color = pal.text_muted, .font_size_px = ts.meta, .font_id = app.fonts.mono, .wrap = false
                            });
                        }
                    }
                }
            }
        }

        auto br = etch::text_input({
            .id = etch::id("btw_input"),
            .buffer = app.btw_input,
            .capacity = sizeof(app.btw_input),
            .placeholder = app.ts("btw.placeholder", "Ask a side question..."),
            .width = etch::Sizing::grow(),
            .height = etch::Sizing::fixed(36.0f),
            .padding = etch::Padding::xy(10.0f, 6.0f),
            .font_size_px = ts.ui_sm,
            .background = pal.bg_input,
            .border_color = pal.border_default,
            .focus_border_color = pal.accent_border,
            .radius = etch::Corners::all(kRadiusSm),
            .submit_on_enter = true,
            .focus_glow_blur = 0.0f,
        });
        if (br.submitted && app.btw_input[0] != '\0') {
            if (!app.showcase_mode) app.client.ask_btw(app.btw_input);
            app.btw_input[0] = '\0';
            etch::set_scroll_stuck_to_bottom(etch::id("btw_scroll"), true);
        }
    }
}

} // namespace

void build_inspector(App& app) {
    const Palette& pal = app.palette();
    const TypeScale ts = app.type();

    ETCH_BOX({
        .id = etch::id("inspector_drawer"),
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .height = etch::Sizing::grow(),
        .background = pal.bg_sidebar,
        .clip_children = true,
        .shrink_width = true,
        .shrink_height = true,
    }) {
        // Refined Workbench Tab Strip (shrink-safe labels + bespoke duotone icons)
        ETCH_BOX({
            .axis = etch::Axis::Row,
            .width = etch::Sizing::grow(),
            .padding = etch::Padding::xy(10.0f, 8.0f),
            .gap = 6.0f,
            .align_y = etch::AlignY::Center,
            .border_color = pal.border_subtle,
            .border_sides = {.bottom = 1.0f},
        }) {
            ETCH_BOX({
                .axis = etch::Axis::Row,
                .width = etch::Sizing::grow(),
                .height = etch::Sizing::fixed(ts.pill_h + 3.0f),
                .padding = etch::Padding::all(2.0f),
                .gap = 2.0f,
                .background = pal.bg_input,
                .radius = etch::Corners::all(kRadiusSm),
                .shrink_width = true,
                .continuous_corners = true,
            }) {
                struct WbTab { const char* key; const char* fallback; uint16_t idle; uint16_t act; };
                const WbTab tabs[4] = {
                    {"workbench.tab.changes", "Changes", app.icons.file_diff, app.icons.file_diff_duotone},
                    {"workbench.tab.agents",  "Agents",  app.icons.agents,    app.icons.agents_duotone},
                    {"workbench.tab.tree",    "Tree",    app.icons.git_graph, app.icons.git_graph_duotone},
                    {"workbench.tab.btw",     "/btw",    app.icons.btw,       app.icons.btw_duotone},
                };
                for (uint32_t i = 0; i < 4; ++i) {
                    const uint32_t tid = etch::id_index(etch::id("ins_tab"), i);
                    const bool active = (static_cast<uint32_t>(app.inspector_tab) == i);
                    const bool hov = etch::hovered(tid);
                    ETCH_BOX({
                        .id = tid,
                        .axis = etch::Axis::Row,
                        .width = etch::Sizing::grow(),
                        .height = etch::Sizing::grow(),
                        .padding = etch::Padding::xy(4.0f, 0.0f),
                        .gap = 5.0f,
                        .align_x = etch::AlignX::Center,
                        .align_y = etch::AlignY::Center,
                        .background = active ? pal.bg_elevated : (hov ? pal.hover : etch::Color::transparent()),
                        .border_color = active ? pal.border_subtle : etch::Color::transparent(),
                        .border_width = active ? 1.0f : 0.0f,
                        .radius = etch::Corners::all(6.0f),
                        .shrink_width = true,
                        .continuous_corners = true,
                    }) {
                        etch::icon({
                            .icon_id = active ? tabs[i].act : tabs[i].idle,
                            .size_px = 13.0f,
                            .color = active ? pal.accent : pal.text_muted,
                        });
                        etch::text(app.ts(tabs[i].key, tabs[i].fallback), {
                            .color = active ? pal.text_primary : pal.text_muted,
                            .font_size_px = ts.ui_sm,
                            .font_id = active ? app.fonts.semibold : app.fonts.body,
                            .wrap = false,
                            .ellipsis = true,
                        });
                    }
                    if (hov) etch::set_cursor(etch::Cursor::Hand);
                    if (etch::clicked(tid)) {
                        app.inspector_tab = static_cast<InspectorTab>(i);
                        if (!app.showcase_mode) {
                            if (i == 1) app.client.refresh_subagents();
                            else if (i == 2) app.client.refresh_tree();
                        }
                    }
                }
            }

            const uint32_t close_id = etch::id("ins_close");
            const bool ch = etch::hovered(close_id);
            ETCH_BOX({
                .id = close_id,
                .width = etch::Sizing::fixed(24.0f),
                .height = etch::Sizing::fixed(24.0f),
                .align_x = etch::AlignX::Center,
                .align_y = etch::AlignY::Center,
                .background = ch ? pal.hover : etch::Color::transparent(),
                .radius = etch::Corners::all(6.0f),
            }) {
                etch::icon({.icon_id = app.icons.close, .size_px = 11.5f, .color = ch ? pal.text_primary : pal.text_muted});
            }
            etch::set_tooltip(close_id, app.ts("workbench.close", "Close Workbench"));
            if (ch) etch::set_cursor(etch::Cursor::Hand);
            if (etch::clicked(close_id)) app.inspector_open = false;
        }

        if (app.inspector_tab == InspectorTab::Changes) build_changes_tab(app, pal);
        else if (app.inspector_tab == InspectorTab::Agents) build_agents_tab(app, pal);
        else if (app.inspector_tab == InspectorTab::Tree) build_tree_tab(app, pal);
        else build_btw_tab(app, pal);
    }
}

} // namespace ompe
