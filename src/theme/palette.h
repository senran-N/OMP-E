#pragma once

#include <etch/etch.h>
#include <inlay/inlay.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ompe {

enum class PaletteId : uint8_t {
    ObsidianBrass = 0, // Warm carbon obsidian + champagne brass + verdigris (Default Dark)
    AlabasterInk  = 1, // Mineral alabaster paper + sienna ochre + forest verdigris (Signature Light)
    CeladonSlate  = 2, // Cool mineral slate + celadon jade + sand (Refined Cool Dark)
    NordicLinen   = 3, // Porcelain linen + botanical spruce + warm graphite (Studio Light)
};

struct Palette {
    PaletteId id = PaletteId::ObsidianBrass;
    const char* name = "Obsidian Brass";
    const char* subtitle = "Warm carbon & brushed brass";
    bool dark = true;

    // 5-level surface ladder
    etch::Color bg_canvas{};      // Main reading backdrop
    etch::Color bg_chrome{};      // Titlebar & outer rail
    etch::Color bg_sidebar{};     // Navigation sidebar
    etch::Color bg_surface{};     // Cards, composer, tool blocks
    etch::Color bg_elevated{};    // Popovers, modals, sheets, active cards
    etch::Color bg_input{};       // Text input wells
    etch::Color bg_code{};        // Code blocks & terminal well

    // Interactive fills
    etch::Color hover{};
    etch::Color active{};
    etch::Color selected{};
    etch::Color selected_subtle{};

    // Hairline borders
    etch::Color border_subtle{};
    etch::Color border_default{};
    etch::Color border_strong{};

    // Typography hierarchy
    etch::Color text_primary{};
    etch::Color text_body{};
    etch::Color text_secondary{};
    etch::Color text_muted{};
    etch::Color text_faint{};

    // Signature semantic accents (OKLCH balanced, zero neon slop)
    etch::Color accent{};           // Primary signature accent
    etch::Color accent_soft{};      // Low-alpha tint fill
    etch::Color accent_border{};    // Subtle accent hairline
    etch::Color on_accent{};        // High-contrast ink on accent button
    etch::Color secondary{};        // Secondary complementary mineral tone
    etch::Color secondary_soft{};
    etch::Color success{};
    etch::Color success_soft{};
    etch::Color warning{};
    etch::Color warning_soft{};
    etch::Color danger{};
    etch::Color danger_soft{};

    // Diff semantic colors (subtle row wash + word-level emphasis)
    etch::Color diff_add_bg{};
    etch::Color diff_add_emph{};
    etch::Color diff_add_fg{};
    etch::Color diff_del_bg{};
    etch::Color diff_del_emph{};
    etch::Color diff_del_fg{};

    // Shadow & scrim
    etch::Color shadow{};
    etch::Color scrim{};

    // Shader linear RGB parameters
    float shader_base[4] = {};
    float shader_accent[4] = {};
    float shader_secondary[4] = {};

    // Convert into matching Etch Theme & InlayDocStyle
    etch::Theme to_etch_theme() const noexcept;
    inlay::DocStyle to_inlay_style(uint16_t font_body, uint16_t font_bold, uint16_t font_mono) const noexcept;
};

const Palette& get_palette(PaletteId id) noexcept;
const char* palette_key(PaletteId id) noexcept;
PaletteId palette_from_key(const char* key) noexcept;
constexpr uint32_t kPaletteCount = 4;

struct FontChoice {
    std::string id;          // "" = bundled default, "harmonyos" = bundled CJK, or OS font family name
    std::string label;       // User-facing display name
    bool is_bundled = false;
};

// Custom font slots registered at startup and hot-reloaded on font preference change
struct FontSlots {
    uint16_t body        = etch::kFontSans;
    uint16_t medium      = etch::kFontMedium;
    uint16_t semibold    = etch::kFontSemiBold;
    uint16_t bold        = etch::kFontBold;
    uint16_t italic      = etch::kFontItalic;
    uint16_t mono        = etch::kFontMono;
    uint16_t mono_medium = etch::kFontMono;
    uint16_t mono_tnum   = etch::kFontMono; // Monospace with OpenType tnum enabled
    uint16_t ui_tnum     = etch::kFontMedium; // UI sans with OpenType tnum enabled
    uint16_t display     = etch::kFontSemiBold;
    uint16_t serif       = etch::kFontBold;   // Editorial high-contrast serif (Instrument Serif) for OMP brand identity
    uint16_t cjk_regular = etch::kInvalidFont;
    uint16_t cjk_medium  = etch::kInvalidFont;
    uint16_t cjk_bold    = etch::kInvalidFont;
    std::string family_label = "System UI";
    bool ui_pref_matched   = true;
    bool mono_pref_matched = true;
};

std::vector<FontChoice> discover_ui_fonts();
std::vector<FontChoice> discover_mono_fonts();

FontSlots load_app_fonts(etch::Context* ctx,
                         std::string_view ui_font_pref = "",
                         std::string_view mono_font_pref = "",
                         const FontSlots* reuse_slots = nullptr) noexcept;

} // namespace ompe
