#pragma once

#include "omp/protocol.h"
#include "theme/palette.h"

#include <etch/etch.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ompe {

inline bool is_mutation_tool_name(std::string_view name) noexcept {
    return name == "edit" || name == "write" || name == "patch" ||
           name == "write_file" || name == "edit_file" ||
           name == "apply_patch" || name == "ast_edit";
}

// Pick best diff source for a tool execution: canonical details.diff / args preview first, fallback to output_text
inline const std::string& best_diff_text(const ToolExecution& te) noexcept {
    return !te.diff_text.empty() ? te.diff_text : te.output_text;
}

enum class DiffLineKind : uint8_t {
    Context = 0,
    Add     = 1,
    Del     = 2,
    Hunk    = 3,
    Header  = 4,
};

struct ParsedDiffLine {
    DiffLineKind kind = DiffLineKind::Context;
    std::string line_num;
    std::string code;
    size_t emph_start = 0; // Byte offset into `code` for intra-line highlight
    size_t emph_end = 0;
};

namespace diff_detail {

inline bool parse_hunk_start_lines(std::string_view line, int& out_old, int& out_new) noexcept {
    // Parse "@@ -42,6 +42,11 @@..."
    const size_t minus = line.find('-');
    const size_t plus = line.find('+');
    if (minus == std::string_view::npos || plus == std::string_view::npos || plus < minus) return false;
    auto parse_int = [](std::string_view s, size_t pos) noexcept -> int {
        int v = 0;
        bool any = false;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
            v = v * 10 + (s[pos] - '0');
            ++pos;
            any = true;
        }
        return any ? v : -1;
    };
    const int o = parse_int(line, minus + 1);
    const int n = parse_int(line, plus + 1);
    if (o < 0 || n < 0) return false;
    out_old = o;
    out_new = n;
    return true;
}

// Match OMP compact line-numbered diffs:
//   Canonical:  "+12|added", "- 4|removed", " 12|context"
//   ast_edit:   "+468│added" (UTF-8 \xE2\x94\x82) or "+468:added"
inline bool try_parse_omp_numbered_line(std::string_view raw, DiffLineKind& kind,
                                        std::string& num, std::string& code) {
    if (raw.size() < 3) return false;
    const char p = raw[0];
    if (p != '+' && p != '-' && p != ' ') return false;

    size_t i = 1;
    while (i < raw.size() && raw[i] == ' ') ++i;
    const size_t dig_start = i;
    while (i < raw.size() && std::isdigit(static_cast<unsigned char>(raw[i]))) ++i;
    if (i == dig_start || (i - dig_start) > 7) return false;

    size_t sep_len = 0;
    if (i < raw.size() && (raw[i] == '|' || raw[i] == ':')) {
        sep_len = 1;
    } else if (i + 2 < raw.size() &&
               static_cast<uint8_t>(raw[i]) == 0xE2u &&
               static_cast<uint8_t>(raw[i + 1]) == 0x94u &&
               static_cast<uint8_t>(raw[i + 2]) == 0x82u) {
        sep_len = 3; // UTF-8 box-drawing vertical bar '│'
    } else {
        return false;
    }

    kind = (p == '+') ? DiffLineKind::Add : ((p == '-') ? DiffLineKind::Del : DiffLineKind::Context);
    num.assign(raw.substr(dig_start, i - dig_start));
    code.assign(raw.substr(i + sep_len));
    return true;
}

inline bool is_utf8_continuation(uint8_t b) noexcept {
    return (b & 0xC0u) == 0x80u;
}

// Score & compute crisp intra-line changed span between a removed line and an added line.
// Returns non-whitespace common character count (0 if lines are not meaningfully related).
inline size_t score_pair_similarity(std::string_view a, std::string_view b,
                                    size_t& out_pre, size_t& out_suf) noexcept {
    if (a.empty() || b.empty() || a == b) return 0;

    // Skip shared leading indentation when measuring meaningful similarity
    size_t indent = 0;
    while (indent < a.size() && indent < b.size() &&
           (a[indent] == ' ' || a[indent] == '\t') && a[indent] == b[indent]) {
        ++indent;
    }

    const size_t min_len = std::min(a.size(), b.size());
    size_t pre = indent;
    while (pre < min_len && a[pre] == b[pre]) ++pre;
    while (pre > indent && is_utf8_continuation(static_cast<uint8_t>(a[pre]))) --pre;

    size_t suf = 0;
    const size_t max_suf = min_len - pre;
    while (suf < max_suf && a[a.size() - 1 - suf] == b[b.size() - 1 - suf]) ++suf;
    while (suf > 0 && (a.size() - suf) < a.size() &&
           is_utf8_continuation(static_cast<uint8_t>(a[a.size() - suf]))) {
        --suf;
    }

    const size_t content_min = (min_len > indent) ? (min_len - indent) : 0;
    const size_t common_non_indent = (pre - indent) + suf;
    if (content_min == 0 || common_non_indent < 4 || common_non_indent * 10 < content_min * 3) {
        return 0;
    }

    out_pre = pre;
    out_suf = suf;
    return common_non_indent;
}

inline void apply_pair_emphasis(ParsedDiffLine& del_line, ParsedDiffLine& add_line,
                                size_t pre, size_t suf) noexcept {
    if (pre < del_line.code.size() - suf) {
        del_line.emph_start = pre;
        del_line.emph_end = del_line.code.size() - suf;
    }
    if (pre < add_line.code.size() - suf) {
        add_line.emph_start = pre;
        add_line.emph_end = add_line.code.size() - suf;
    }
}

} // namespace diff_detail

inline std::vector<ParsedDiffLine> parse_structured_diff(const std::string& diff_text, int max_lines = 400) {
    std::vector<ParsedDiffLine> out;
    if (diff_text.empty()) return out;

    std::istringstream ss(diff_text);
    std::string raw;
    int old_ln = -1;
    int new_ln = -1;

    while (std::getline(ss, raw) && static_cast<int>(out.size()) < max_lines) {
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();

        // Trim leading whitespace only to check ellipsis separator
        std::string_view sv(raw);
        std::string_view trimmed = sv;
        while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) trimmed.remove_prefix(1);
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) trimmed.remove_suffix(1);

        if (trimmed == "\xE2\x80\xA6" || trimmed == "...") {
            out.push_back({DiffLineKind::Hunk, "", "···", 0, 0});
            continue;
        }

        if (sv.rfind("@@", 0) == 0) {
            diff_detail::parse_hunk_start_lines(sv, old_ln, new_ln);
            out.push_back({DiffLineKind::Hunk, "", raw, 0, 0});
            continue;
        }

        if (sv.rfind("+++", 0) == 0 || sv.rfind("---", 0) == 0 ||
            sv.rfind("diff --git ", 0) == 0 || sv.rfind("index ", 0) == 0 ||
            sv.rfind("# ", 0) == 0) {
            out.push_back({DiffLineKind::Header, "", raw, 0, 0});
            continue;
        }

        // 1. Check OMP canonical / ast_edit compact line-numbered format first!
        ParsedDiffLine pl;
        if (diff_detail::try_parse_omp_numbered_line(sv, pl.kind, pl.line_num, pl.code)) {
            out.push_back(std::move(pl));
            continue;
        }

        // 2. Classic unified diff lines (+ / - / context)
        if (!sv.empty() && sv[0] == '+') {
            pl.kind = DiffLineKind::Add;
            if (new_ln >= 0) pl.line_num = std::to_string(new_ln++);
            pl.code = raw.substr(1);
            out.push_back(std::move(pl));
        } else if (!sv.empty() && sv[0] == '-') {
            pl.kind = DiffLineKind::Del;
            if (old_ln >= 0) pl.line_num = std::to_string(old_ln++);
            pl.code = raw.substr(1);
            out.push_back(std::move(pl));
        } else {
            pl.kind = DiffLineKind::Context;
            if (new_ln >= 0) {
                pl.line_num = std::to_string(new_ln++);
                if (old_ln >= 0) ++old_ln;
            }
            pl.code = (!raw.empty() && raw[0] == ' ') ? raw.substr(1) : raw;
            out.push_back(std::move(pl));
        }
    }

    // Compute best-match intra-line word/token emphasis across adjacent Del -> Add runs
    size_t i = 0;
    while (i < out.size()) {
        if (out[i].kind != DiffLineKind::Del) { ++i; continue; }
        const size_t del_start = i;
        while (i < out.size() && out[i].kind == DiffLineKind::Del) ++i;
        const size_t del_count = i - del_start;
        const size_t add_start = i;
        while (i < out.size() && out[i].kind == DiffLineKind::Add) ++i;
        const size_t add_count = i - add_start;
        if (add_count == 0) continue;

        // Monotonic greedy best-match pairing (bounded window for O(N) safety)
        size_t next_add = 0;
        const size_t lim_del = std::min<size_t>(del_count, 24);
        const size_t lim_add = std::min<size_t>(add_count, 24);
        for (size_t di = 0; di < lim_del && next_add < lim_add; ++di) {
            size_t best_ai = lim_add;
            size_t best_score = 0, best_pre = 0, best_suf = 0;
            for (size_t ai = next_add; ai < lim_add; ++ai) {
                size_t pre = 0, suf = 0;
                const size_t sc = diff_detail::score_pair_similarity(
                    out[del_start + di].code, out[add_start + ai].code, pre, suf);
                if (sc > best_score) {
                    best_score = sc;
                    best_ai = ai;
                    best_pre = pre;
                    best_suf = suf;
                }
            }
            if (best_score > 0 && best_ai < lim_add) {
                diff_detail::apply_pair_emphasis(out[del_start + di], out[add_start + best_ai], best_pre, best_suf);
                next_add = best_ai + 1;
            }
        }
    }

    return out;
}

inline std::pair<int, int> count_diff_stats(const std::string& diff_text) {
    if (diff_text.empty()) return {0, 0};
    int adds = 0, dels = 0;
    std::istringstream ss(diff_text);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line[0] == '+' && line.rfind("+++", 0) != 0) ++adds;
        else if (line[0] == '-' && line.rfind("---", 0) != 0) ++dels;
    }
    return {adds, dels};
}

inline bool looks_like_structured_diff(const std::string& text) noexcept {
    if (text.empty()) return false;
    const auto [a, d] = count_diff_stats(text);
    return (a > 0 || d > 0 || text.find("@@") != std::string::npos);
}

// Publication-grade 3-column structured diff renderer:
//   [2.5px status accent] [40px tabular line # gutter] [18px +/- sign] [code w/ intra-line emphasis]
inline void render_structured_diff(const std::string& diff_text, const Palette& pal,
                                   const FontSlots& fonts, int max_lines = 48,
                                   float font_size_px = 13.0f, bool wrap_lines = true) {
    if (diff_text.empty()) return;
    const std::vector<ParsedDiffLine> lines = parse_structured_diff(diff_text, max_lines);
    if (lines.empty()) return;

    bool has_any_line_num = false;
    for (const auto& l : lines) {
        if (!l.line_num.empty()) { has_any_line_num = true; break; }
    }

    const etch::Color gutter_sep = pal.dark ? etch::Color::rgba8(244, 238, 226, 12)
                                            : etch::Color::rgba8(28, 25, 21, 14);
    const etch::Color hunk_bg = pal.dark ? etch::Color::rgba8(pal.accent.r, pal.accent.g, pal.accent.b, 18)
                                         : etch::Color::rgba8(pal.accent.r, pal.accent.g, pal.accent.b, 14);
    const etch::Color add_num_col = etch::Color::rgba8(pal.success.r, pal.success.g, pal.success.b, pal.dark ? 195 : 210);
    const etch::Color del_num_col = etch::Color::rgba8(pal.danger.r, pal.danger.g, pal.danger.b, pal.dark ? 195 : 210);

    ETCH_BOX({
        .axis = etch::Axis::Col,
        .width = etch::Sizing::grow(),
        .padding = etch::Padding::xy(0.0f, 4.0f),
        .gap = 0.0f,
        .background = pal.bg_code,
        .clip_children = true,
    }) {
        for (size_t idx = 0; idx < lines.size(); ++idx) {
            const ParsedDiffLine& pl = lines[idx];

            if (pl.kind == DiffLineKind::Hunk) {
                const bool is_ellipsis = (pl.code == "···");
                ETCH_BOX({
                    .axis = etch::Axis::Row,
                    .width = etch::Sizing::grow(),
                    .padding = etch::Padding::xy(10.0f, is_ellipsis ? 2.0f : 3.5f),
                    .align_y = etch::AlignY::Center,
                    .background = is_ellipsis ? pal.bg_surface : hunk_bg,
                    .border_color = pal.border_subtle,
                    .border_sides = {.top = (idx > 0 ? 1.0f : 0.0f), .bottom = 1.0f},
                }) {
                    etch::text(etch::str_cstr(pl.code.c_str()), {
                        .color = is_ellipsis ? pal.text_faint : pal.accent,
                        .font_size_px = font_size_px - 1.0f,
                        .font_id = fonts.mono_tnum,
                        .wrap = false,
                        .ellipsis = true,
                    });
                }
                continue;
            }

            if (pl.kind == DiffLineKind::Header) {
                ETCH_BOX({
                    .axis = etch::Axis::Row,
                    .width = etch::Sizing::grow(),
                    .padding = etch::Padding::xy(10.0f, 3.0f),
                    .align_y = etch::AlignY::Center,
                    .background = pal.bg_surface,
                    .border_color = pal.border_subtle,
                    .border_sides = {.bottom = 1.0f},
                }) {
                    etch::text(etch::str_cstr(pl.code.c_str()), {
                        .color = pal.text_muted,
                        .font_size_px = font_size_px - 0.8f,
                        .font_id = fonts.mono,
                        .wrap = false,
                        .ellipsis = true,
                    });
                }
                continue;
            }

            const bool is_add = (pl.kind == DiffLineKind::Add);
            const bool is_del = (pl.kind == DiffLineKind::Del);
            const etch::Color row_bg = is_add ? pal.diff_add_bg : (is_del ? pal.diff_del_bg : etch::Color::transparent());
            const etch::Color bar_col = is_add ? pal.success : (is_del ? pal.danger : etch::Color::transparent());
            const etch::Color fg = is_add ? pal.diff_add_fg : (is_del ? pal.diff_del_fg : pal.text_secondary);
            const etch::Color emph_bg = is_add ? pal.diff_add_emph : pal.diff_del_emph;
            const etch::Color num_col = is_add ? add_num_col : (is_del ? del_num_col : pal.text_faint);
            const char* sign_str = is_add ? "+" : (is_del ? "-" : " ");
            const etch::Color sign_col = is_add ? pal.success : (is_del ? pal.danger : etch::Color::transparent());

            ETCH_BOX({
                .axis = etch::Axis::Row,
                .width = etch::Sizing::grow(),
                .padding = {.left = 0.0f, .right = 10.0f, .top = 1.5f, .bottom = 1.5f},
                .gap = 0.0f,
                .align_y = etch::AlignY::Top,
                .background = row_bg,
                .border_color = bar_col,
                .border_sides = {.left = 2.5f},
            }) {
                // Column 1: Right-aligned tabular line number gutter
                if (has_any_line_num) {
                    ETCH_BOX({
                        .width = etch::Sizing::fixed(42.0f),
                        .padding = {.left = 4.0f, .right = 8.0f, .top = 0.5f, .bottom = 0.0f},
                        .align_x = etch::AlignX::Right,
                        .border_color = gutter_sep,
                        .border_sides = {.right = 1.0f},
                    }) {
                        etch::text(etch::str_cstr(pl.line_num.empty() ? " " : pl.line_num.c_str()), {
                            .color = num_col,
                            .font_size_px = font_size_px - 1.5f,
                            .font_id = fonts.mono_tnum,
                            .wrap = false,
                        });
                    }
                }

                // Column 2: Dedicated +/- sign rail
                ETCH_BOX({
                    .width = etch::Sizing::fixed(18.0f),
                    .align_x = etch::AlignX::Center,
                }) {
                    etch::text(etch::str_cstr(sign_str), {
                        .color = sign_col,
                        .font_size_px = font_size_px,
                        .font_id = fonts.mono,
                        .wrap = false,
                    });
                }

                // Column 3: Code content with optional intra-line changed-span emphasis
                ETCH_BOX({
                    .width = etch::Sizing::grow(),
                    .shrink_width = true,
                }) {
                    if (pl.emph_end > pl.emph_start && pl.emph_end <= pl.code.size()) {
                        etch::TextSpan spans[3]{};
                        size_t n = 0;
                        if (pl.emph_start > 0) {
                            spans[n++] = {
                                .text = etch::Str{pl.code.data(), static_cast<uint32_t>(pl.emph_start), false},
                                .color = fg,
                                .font_size_px = font_size_px,
                                .font_id = fonts.mono,
                            };
                        }
                        spans[n++] = {
                            .text = etch::Str{pl.code.data() + pl.emph_start,
                                              static_cast<uint32_t>(pl.emph_end - pl.emph_start), false},
                            .color = pal.dark ? pal.text_primary : fg,
                            .background = emph_bg,
                            .font_size_px = font_size_px,
                            .font_id = fonts.mono,
                            .padding_x = 1.5f,
                            .bg_radius = 2.5f,
                        };
                        if (pl.emph_end < pl.code.size()) {
                            spans[n++] = {
                                .text = etch::Str{pl.code.data() + pl.emph_end,
                                                  static_cast<uint32_t>(pl.code.size() - pl.emph_end), false},
                                .color = fg,
                                .font_size_px = font_size_px,
                                .font_id = fonts.mono,
                            };
                        }
                        etch::text_spans(std::span<const etch::TextSpan>(spans, n), {
                            .color = fg,
                            .font_size_px = font_size_px,
                            .font_id = fonts.mono,
                            .wrap = wrap_lines,
                            .selectable = true,
                        });
                    } else {
                        etch::text(etch::str_cstr(pl.code.empty() ? " " : pl.code.c_str()), {
                            .color = fg,
                            .font_size_px = font_size_px,
                            .font_id = fonts.mono,
                            .wrap = wrap_lines,
                            .selectable = true,
                        });
                    }
                }
            }
        }
    }
}

} // namespace ompe
