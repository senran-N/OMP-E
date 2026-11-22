#pragma once

#include <algorithm>
#include <cmath>

namespace ompe {

// Unified, CSS-calibrated typographic ramp driven by DesktopPrefs::font_size_px (default 15.0px).
// Paired with optical scale_adjust in Etch (Geist + Geist Mono + HarmonyOS Sans SC),
// every size below maps 1:1 to true CSS em pixels (matching Linear / Vercel / Zed / Claude Desktop).
struct TypeScale {
    float scale   = 1.0f;  // body_px / 15.0f, clamped [0.85, 1.35]
    float display = 30.0f; // Welcome hero headline
    float h1      = 20.0f; // Modal / Settings view titles
    float h2      = 15.5f; // Section / Card primary titles
    float body    = 15.0f; // Chat prose, Markdown, Composer input
    float ui      = 13.5f; // Sidebar rows, Titlebar, Popover titles, Settings labels, Inputs
    float ui_sm   = 12.5f; // Pills, Exploration trace, Plan steps, Descriptions, Secondary controls
    float code    = 13.0f; // Diffs, Terminal, File paths, Inline code
    float meta    = 11.5f; // Timestamps, +N/-M stats, Diff line numbers, Keycaps, Badges
    float micro   = 10.5f; // Uppercase tracked section dividers (THREADS, MODIFIED FILES, CORE)

    // Proportional control dimensions so larger zoom levels stay airy and never clip
    float row_h   = 34.0f;
    float pill_h  = 29.0f;
    float card_hdr_h = 37.0f;
    float term_cell_w = 7.9f;
    float term_line_h = 19.5f;

    static constexpr float kDefaultBodyPx = 15.0f;
    static constexpr float kMinBodyPx     = 13.0f;
    static constexpr float kMaxBodyPx     = 20.0f;

    static TypeScale from_body(float body_px) noexcept {
        const float clamped = std::clamp(body_px, kMinBodyPx, kMaxBodyPx);
        const float s = std::clamp(clamped / kDefaultBodyPx, 0.85f, 1.35f);
        auto r = [](float v) noexcept -> float {
            return std::round(v * 2.0f) * 0.5f; // Snap to crisp 0.5px increments
        };
        TypeScale t{};
        t.scale      = s;
        t.display    = r(30.0f * s);
        t.h1         = r(20.0f * s);
        t.h2         = r(15.5f * s);
        t.body       = r(clamped);
        t.ui         = r(13.5f * s);
        t.ui_sm      = r(12.5f * s);
        t.code       = r(13.0f * s);
        t.meta       = r(11.5f * s);
        t.micro      = r(10.5f * s);
        t.row_h      = r(34.0f * s);
        t.pill_h     = r(29.0f * s);
        t.card_hdr_h = r(37.0f * s);
        t.term_cell_w = 7.9f * s;
        t.term_line_h = r(19.5f * s);
        return t;
    }
};

} // namespace ompe
