#pragma once

#include "core/i18n.h"
#include "theme/palette.h"

#include <etch/etch.h>

#include <string_view>

namespace ompe {

struct ThinkingLevelMeta {
    const char* short_label;
    const char* title;
    const char* description;
    int bars;         // 0..4 lit bars
    bool cool_tint;   // Use mineral secondary for low/minimal, signature accent for medium+
    bool peak_gleam;  // Highlight peak extra-high intensity
};

inline ThinkingLevelMeta thinking_level_meta(std::string_view level, const I18nCatalog& i18n) noexcept {
    if (level == "minimal") {
        return {
            i18n.get("thinking.minimal.short", "Minimal"),
            i18n.get("thinking.minimal.title", "Minimal"),
            i18n.get("thinking.minimal.desc",  "Brief check · near-instant"),
            1, true, false
        };
    }
    if (level == "low") {
        return {
            i18n.get("thinking.low.short", "Low"),
            i18n.get("thinking.low.title", "Low"),
            i18n.get("thinking.low.desc",  "Light scratchpad · fast edits"),
            1, true, false
        };
    }
    if (level == "medium") {
        return {
            i18n.get("thinking.medium.short", "Medium"),
            i18n.get("thinking.medium.title", "Medium"),
            i18n.get("thinking.medium.desc",  "Balanced depth and latency"),
            2, false, false
        };
    }
    if (level == "high") {
        return {
            i18n.get("thinking.high.short", "High"),
            i18n.get("thinking.high.title", "High"),
            i18n.get("thinking.high.desc",  "Deep multi-step engineering"),
            3, false, false
        };
    }
    if (level == "xhigh") {
        return {
            i18n.get("thinking.xhigh.short", "XHigh"),
            i18n.get("thinking.xhigh.title", "Extra High"),
            i18n.get("thinking.xhigh.desc",  "Maximum reasoning budget"),
            4, false, true
        };
    }
    return {
        i18n.get("thinking.off.short", "Off"),
        i18n.get("thinking.off.title", "Off"),
        i18n.get("thinking.off.desc",  "Direct response · lowest latency"),
        0, false, false
    };
}

// Crisp 4-bar stepped intensity meter (Dieter Rams / precision instrument glyph)
inline void render_thinking_bars(const ThinkingLevelMeta& meta, const Palette& pal,
                                 bool muted_override = false) noexcept {
    static constexpr float kHeights[4] = {4.5f, 7.0f, 9.5f, 12.0f};
    const etch::Color unlit = pal.dark ? etch::Color::rgba8(244, 238, 226, 32)
                                       : etch::Color::rgba8(28, 25, 21, 30);
    const etch::Color lit_base = muted_override
        ? pal.text_faint
        : (meta.cool_tint ? pal.secondary : pal.accent);

    ETCH_BOX({
        .axis = etch::Axis::Row,
        .width = etch::Sizing::fixed(16.0f),
        .height = etch::Sizing::fixed(13.0f),
        .gap = 2.0f,
        .align_x = etch::AlignX::Center,
        .align_y = etch::AlignY::Bottom,
    }) {
        for (int i = 0; i < 4; ++i) {
            const bool lit = (i < meta.bars);
            etch::Color col = unlit;
            if (lit) {
                // Subtle luminance progression across lit bars
                if (meta.peak_gleam && i == 3) {
                    col = pal.accent;
                } else {
                    const uint8_t alpha = static_cast<uint8_t>(185 + i * 23);
                    col = etch::Color::rgba8(lit_base.r, lit_base.g, lit_base.b, alpha);
                }
            }
            etch::box({
                .width = etch::Sizing::fixed(2.5f),
                .height = etch::Sizing::fixed(kHeights[i]),
                .background = col,
                .radius = etch::Corners::all(1.25f),
            });
        }
    }
}

} // namespace ompe
