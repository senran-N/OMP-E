#include "theme/palette.h"

#include <SDL3/SDL_filesystem.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ompe {
namespace {

void srgb_to_linear_f4(etch::Color c, float w, float out[4]) noexcept {
    const auto to_lin = [](uint8_t u) noexcept -> float {
        const float s = static_cast<float>(u) / 255.0f;
        return (s <= 0.04045f) ? (s / 12.92f) : std::pow((s + 0.055f) / 1.055f, 2.4f);
    };
    out[0] = to_lin(c.r);
    out[1] = to_lin(c.g);
    out[2] = to_lin(c.b);
    out[3] = w;
}

Palette make_obsidian_brass() noexcept {
    using etch::Color;
    Palette p{};
    p.id = PaletteId::ObsidianBrass;
    p.name = "Obsidian Brass";
    p.subtitle = "Warm carbon & champagne brass";
    p.dark = true;

    p.bg_canvas       = Color::rgb_hex(0x0E0E10);
    p.bg_chrome       = Color::rgb_hex(0x0A0A0C);
    p.bg_sidebar      = Color::rgb_hex(0x111114);
    p.bg_surface      = Color::rgb_hex(0x16161A);
    p.bg_elevated     = Color::rgb_hex(0x1D1D22);
    p.bg_input        = Color::rgb_hex(0x131317);
    p.bg_code         = Color::rgb_hex(0x0C0C0F);

    p.hover           = Color::rgba8(244, 238, 226, 14);
    p.active          = Color::rgba8(244, 238, 226, 22);
    p.selected        = Color::rgba8(216, 166, 87, 26);
    p.selected_subtle = Color::rgba8(216, 166, 87, 14);

    p.border_subtle   = Color::rgba8(244, 238, 226, 15);
    p.border_default  = Color::rgba8(244, 238, 226, 26);
    p.border_strong   = Color::rgba8(244, 238, 226, 44);

    p.text_primary    = Color::rgb_hex(0xF3EFE8);
    p.text_body       = Color::rgb_hex(0xDFDAD0);
    p.text_secondary  = Color::rgb_hex(0xB4ADA2);
    p.text_muted      = Color::rgb_hex(0x888177);
    p.text_faint      = Color::rgb_hex(0x5E5952);

    p.accent          = Color::rgb_hex(0xD8A657); // Brushed champagne brass
    p.accent_soft     = Color::rgba8(216, 166, 87, 28);
    p.accent_border   = Color::rgba8(216, 166, 87, 88);
    p.on_accent       = Color::rgb_hex(0x14110B);
    p.secondary       = Color::rgb_hex(0x6BA89B); // Mineral verdigris
    p.secondary_soft  = Color::rgba8(107, 168, 155, 24);
    p.success         = Color::rgb_hex(0x6EB88D);
    p.success_soft    = Color::rgba8(110, 184, 141, 24);
    p.warning         = Color::rgb_hex(0xE0A854);
    p.warning_soft    = Color::rgba8(224, 168, 84, 26);
    p.danger          = Color::rgb_hex(0xD96B59);
    p.danger_soft     = Color::rgba8(217, 107, 89, 28);

    p.diff_add_bg     = Color::rgba8(94, 184, 134, 34);
    p.diff_add_emph   = Color::rgba8(94, 184, 134, 78);
    p.diff_add_fg     = Color::rgb_hex(0x9AE0B6);
    p.diff_del_bg     = Color::rgba8(217, 102, 86, 34);
    p.diff_del_emph   = Color::rgba8(217, 102, 86, 80);
    p.diff_del_fg     = Color::rgb_hex(0xF29B8D);

    p.shadow          = Color::rgba8(0, 0, 0, 150);
    p.scrim           = Color::rgba8(6, 6, 8, 165);

    srgb_to_linear_f4(p.bg_canvas, 1.0f, p.shader_base);
    srgb_to_linear_f4(p.accent, 0.0f, p.shader_accent);
    srgb_to_linear_f4(p.secondary, 0.0f, p.shader_secondary);
    return p;
}

Palette make_alabaster_ink() noexcept {
    using etch::Color;
    Palette p{};
    p.id = PaletteId::AlabasterInk;
    p.name = "Alabaster Ink";
    p.subtitle = "Warm mineral paper & terracotta";
    p.dark = false;

    p.bg_canvas       = Color::rgb_hex(0xF6F3EC);
    p.bg_chrome       = Color::rgb_hex(0xECE8DE);
    p.bg_sidebar      = Color::rgb_hex(0xF0ECE3);
    p.bg_surface      = Color::rgb_hex(0xFCFBF8);
    p.bg_elevated     = Color::rgb_hex(0xFFFFFF);
    p.bg_input        = Color::rgb_hex(0xFAF8F3);
    p.bg_code         = Color::rgb_hex(0xF2EFE7);

    p.hover           = Color::rgba8(28, 25, 21, 10);
    p.active          = Color::rgba8(28, 25, 21, 18);
    p.selected        = Color::rgba8(182, 99, 53, 22);
    p.selected_subtle = Color::rgba8(182, 99, 53, 12);

    p.border_subtle   = Color::rgba8(32, 28, 22, 16);
    p.border_default  = Color::rgba8(32, 28, 22, 28);
    p.border_strong   = Color::rgba8(32, 28, 22, 48);

    p.text_primary    = Color::rgb_hex(0x1B1916);
    p.text_body       = Color::rgb_hex(0x2E2A25);
    p.text_secondary  = Color::rgb_hex(0x575148);
    p.text_muted      = Color::rgb_hex(0x7B746A);
    p.text_faint      = Color::rgb_hex(0xA69E92);

    p.accent          = Color::rgb_hex(0xB66335); // Burnished sienna terracotta
    p.accent_soft     = Color::rgba8(182, 99, 53, 20);
    p.accent_border   = Color::rgba8(182, 99, 53, 76);
    p.on_accent       = Color::rgb_hex(0xFFFDF9);
    p.secondary       = Color::rgb_hex(0x2C7466); // Forest verdigris
    p.secondary_soft  = Color::rgba8(44, 116, 102, 18);
    p.success         = Color::rgb_hex(0x2B7A54);
    p.success_soft    = Color::rgba8(43, 122, 84, 18);
    p.warning         = Color::rgb_hex(0xB5791E);
    p.warning_soft    = Color::rgba8(181, 121, 30, 20);
    p.danger          = Color::rgb_hex(0xBA4334);
    p.danger_soft     = Color::rgba8(186, 67, 52, 20);

    p.diff_add_bg     = Color::rgba8(43, 122, 84, 28);
    p.diff_add_emph   = Color::rgba8(43, 122, 84, 62);
    p.diff_add_fg     = Color::rgb_hex(0x1B5839);
    p.diff_del_bg     = Color::rgba8(186, 67, 52, 28);
    p.diff_del_emph   = Color::rgba8(186, 67, 52, 62);
    p.diff_del_fg     = Color::rgb_hex(0x8F2B1F);

    p.shadow          = Color::rgba8(34, 27, 18, 28);
    p.scrim           = Color::rgba8(24, 21, 17, 92);

    srgb_to_linear_f4(p.bg_canvas, 0.0f, p.shader_base);
    srgb_to_linear_f4(p.accent, 0.0f, p.shader_accent);
    srgb_to_linear_f4(p.secondary, 0.0f, p.shader_secondary);
    return p;
}

Palette make_celadon_slate() noexcept {
    using etch::Color;
    Palette p{};
    p.id = PaletteId::CeladonSlate;
    p.name = "Celadon Slate";
    p.subtitle = "Mineral dark slate & jade mist";
    p.dark = true;

    p.bg_canvas       = Color::rgb_hex(0x0C1113);
    p.bg_chrome       = Color::rgb_hex(0x080C0E);
    p.bg_sidebar      = Color::rgb_hex(0x0F1518);
    p.bg_surface      = Color::rgb_hex(0x141C20);
    p.bg_elevated     = Color::rgb_hex(0x1B252A);
    p.bg_input        = Color::rgb_hex(0x11181C);
    p.bg_code         = Color::rgb_hex(0x0A0F11);

    p.hover           = Color::rgba8(224, 240, 236, 13);
    p.active          = Color::rgba8(224, 240, 236, 22);
    p.selected        = Color::rgba8(124, 196, 174, 26);
    p.selected_subtle = Color::rgba8(124, 196, 174, 14);

    p.border_subtle   = Color::rgba8(216, 234, 230, 15);
    p.border_default  = Color::rgba8(216, 234, 230, 26);
    p.border_strong   = Color::rgba8(216, 234, 230, 44);

    p.text_primary    = Color::rgb_hex(0xE9F0EE);
    p.text_body       = Color::rgb_hex(0xD2DEDA);
    p.text_secondary  = Color::rgb_hex(0xA0B3AE);
    p.text_muted      = Color::rgb_hex(0x748984);
    p.text_faint      = Color::rgb_hex(0x4E605C);

    p.accent          = Color::rgb_hex(0x7CC4AE); // Celadon jade
    p.accent_soft     = Color::rgba8(124, 196, 174, 26);
    p.accent_border   = Color::rgba8(124, 196, 174, 84);
    p.on_accent       = Color::rgb_hex(0x0A1512);
    p.secondary       = Color::rgb_hex(0xD4B483); // Warm sand
    p.secondary_soft  = Color::rgba8(212, 180, 131, 22);
    p.success         = Color::rgb_hex(0x73C29B);
    p.success_soft    = Color::rgba8(115, 194, 155, 22);
    p.warning         = Color::rgb_hex(0xDFB062);
    p.warning_soft    = Color::rgba8(223, 176, 98, 24);
    p.danger          = Color::rgb_hex(0xDA7366);
    p.danger_soft     = Color::rgba8(218, 115, 102, 26);

    p.diff_add_bg     = Color::rgba8(100, 194, 152, 34);
    p.diff_add_emph   = Color::rgba8(100, 194, 152, 78);
    p.diff_add_fg     = Color::rgb_hex(0x9EE2C0);
    p.diff_del_bg     = Color::rgba8(218, 110, 98, 34);
    p.diff_del_emph   = Color::rgba8(218, 110, 98, 80);
    p.diff_del_fg     = Color::rgb_hex(0xF5A095);

    p.shadow          = Color::rgba8(0, 0, 0, 155);
    p.scrim           = Color::rgba8(4, 8, 10, 165);

    srgb_to_linear_f4(p.bg_canvas, 1.0f, p.shader_base);
    srgb_to_linear_f4(p.accent, 0.0f, p.shader_accent);
    srgb_to_linear_f4(p.secondary, 0.0f, p.shader_secondary);
    return p;
}

Palette make_nordic_linen() noexcept {
    using etch::Color;
    Palette p{};
    p.id = PaletteId::NordicLinen;
    p.name = "Nordic Linen";
    p.subtitle = "Porcelain surface & deep spruce";
    p.dark = false;

    p.bg_canvas       = Color::rgb_hex(0xF3F5F4);
    p.bg_chrome       = Color::rgb_hex(0xE8ECEA);
    p.bg_sidebar      = Color::rgb_hex(0xECF0EE);
    p.bg_surface      = Color::rgb_hex(0xFAFCFB);
    p.bg_elevated     = Color::rgb_hex(0xFFFFFF);
    p.bg_input        = Color::rgb_hex(0xF7FAF9);
    p.bg_code         = Color::rgb_hex(0xEEF2F0);

    p.hover           = Color::rgba8(20, 34, 30, 10);
    p.active          = Color::rgba8(20, 34, 30, 18);
    p.selected        = Color::rgba8(42, 111, 98, 20);
    p.selected_subtle = Color::rgba8(42, 111, 98, 11);

    p.border_subtle   = Color::rgba8(22, 36, 32, 16);
    p.border_default  = Color::rgba8(22, 36, 32, 28);
    p.border_strong   = Color::rgba8(22, 36, 32, 46);

    p.text_primary    = Color::rgb_hex(0x16201D);
    p.text_body       = Color::rgb_hex(0x283531);
    p.text_secondary  = Color::rgb_hex(0x50615B);
    p.text_muted      = Color::rgb_hex(0x72837D);
    p.text_faint      = Color::rgb_hex(0x9BA9A4);

    p.accent          = Color::rgb_hex(0x2A6F62); // Botanical spruce
    p.accent_soft     = Color::rgba8(42, 111, 98, 18);
    p.accent_border   = Color::rgba8(42, 111, 98, 72);
    p.on_accent       = Color::rgb_hex(0xF7FCFA);
    p.secondary       = Color::rgb_hex(0x9F6B3B); // Warm walnut
    p.secondary_soft  = Color::rgba8(159, 107, 59, 18);
    p.success         = Color::rgb_hex(0x2A7856);
    p.success_soft    = Color::rgba8(42, 120, 86, 18);
    p.warning         = Color::rgb_hex(0xAC7720);
    p.warning_soft    = Color::rgba8(172, 119, 32, 18);
    p.danger          = Color::rgb_hex(0xB8463A);
    p.danger_soft     = Color::rgba8(184, 70, 58, 18);

    p.diff_add_bg     = Color::rgba8(42, 120, 86, 26);
    p.diff_add_emph   = Color::rgba8(42, 120, 86, 60);
    p.diff_add_fg     = Color::rgb_hex(0x1A543B);
    p.diff_del_bg     = Color::rgba8(184, 70, 58, 26);
    p.diff_del_emph   = Color::rgba8(184, 70, 58, 60);
    p.diff_del_fg     = Color::rgb_hex(0x882C22);

    p.shadow          = Color::rgba8(16, 28, 24, 24);
    p.scrim           = Color::rgba8(16, 24, 22, 86);

    srgb_to_linear_f4(p.bg_canvas, 0.0f, p.shader_base);
    srgb_to_linear_f4(p.accent, 0.0f, p.shader_accent);
    srgb_to_linear_f4(p.secondary, 0.0f, p.shader_secondary);
    return p;
}

} // namespace

const Palette& get_palette(PaletteId id) noexcept {
    static const Palette kPalettes[kPaletteCount] = {
        make_obsidian_brass(),
        make_alabaster_ink(),
        make_celadon_slate(),
        make_nordic_linen(),
    };
    const uint8_t idx = static_cast<uint8_t>(id);
    return kPalettes[(idx < kPaletteCount) ? idx : 0];
}

const char* palette_key(PaletteId id) noexcept {
    switch (id) {
        case PaletteId::ObsidianBrass: return "obsidian-brass";
        case PaletteId::AlabasterInk:  return "alabaster-ink";
        case PaletteId::CeladonSlate:  return "celadon-slate";
        case PaletteId::NordicLinen:   return "nordic-linen";
    }
    return "obsidian-brass";
}

PaletteId palette_from_key(const char* key) noexcept {
    if (!key) return PaletteId::ObsidianBrass;
    if (std::strcmp(key, "alabaster-ink") == 0) return PaletteId::AlabasterInk;
    if (std::strcmp(key, "celadon-slate") == 0) return PaletteId::CeladonSlate;
    if (std::strcmp(key, "nordic-linen") == 0)  return PaletteId::NordicLinen;
    return PaletteId::ObsidianBrass;
}

etch::Theme Palette::to_etch_theme() const noexcept {
    etch::Theme t = dark ? etch::Theme::dark() : etch::Theme::light();
    t.id = etch::ThemeId::Custom;
    t.bg_canvas = bg_canvas;
    t.bg_surface = bg_surface;
    t.bg_elevated = bg_elevated;
    t.bg_chrome = bg_chrome;
    t.bg_popup = bg_elevated;
    t.bg_tooltip = bg_elevated;
    t.bg_input = bg_input;
    t.bg_table = bg_surface;
    t.bg_node_canvas = bg_code;
    t.surface_control = bg_elevated;
    t.surface_hover = hover;
    t.surface_active = active;
    t.surface_disabled = bg_sidebar;
    t.surface_subtle = bg_surface;
    t.surface_subtle_hover = bg_elevated;
    t.surface_subtle_body = bg_code;
    t.surface_selected = selected;
    t.text_primary = text_primary;
    t.text_secondary = text_secondary;
    t.text_body = text_body;
    t.text_code = text_body;
    t.text_muted = text_muted;
    t.text_disabled = text_faint;
    t.accent = accent;
    t.accent_secondary = secondary;
    t.accent_primary = accent;
    t.on_primary = on_accent;
    t.focus_ring = accent;
    t.danger = danger;
    t.warning = warning;
    t.border_subtle = border_subtle;
    t.border_line = border_subtle;
    t.border_input = border_default;
    t.border_node = border_default;
    t.border_control = border_default;
    t.border_tooltip = border_strong;
    t.selection = accent_soft;
    t.selection_hover = selected_subtle;
    t.backdrop = scrim;
    t.shadow = shadow;
    t.scrollbar_track = etch::Color::transparent();
    t.scrollbar_thumb = dark ? etch::Color::rgba8(220, 214, 204, 52) : etch::Color::rgba8(40, 36, 30, 48);
    t.scrollbar_thumb_hover = dark ? etch::Color::rgba8(220, 214, 204, 96) : etch::Color::rgba8(40, 36, 30, 88);
    t.scrollbar_thumb_drag = accent;
    t.grid_minor = border_subtle;
    t.grid_major = border_default;
    t.canvas_border = border_subtle;
    t.syntax_comment = text_muted;
    t.syntax_string = secondary;
    t.syntax_number = accent;
    t.syntax_keyword = dark ? etch::Color::rgb_hex(0xD89B82) : etch::Color::rgb_hex(0x8D4B63);
    t.radius_small_px = 6.0f;
    t.radius_control_px = 8.0f;
    t.radius_table_px = 10.0f;
    t.radius_input_px = 12.0f;
    t.radius_panel_px = 12.0f;
    t.radius_pill_px = 999.0f;
    return t;
}

inlay::DocStyle Palette::to_inlay_style(uint16_t font_body, uint16_t font_bold, uint16_t font_mono) const noexcept {
    inlay::DocStyle s = inlay::doc_style_default();
    const auto c = [](etch::Color col) noexcept -> EtchColor {
        return EtchColor{col.r, col.g, col.b, col.a};
    };
    s.has_background = false;
    s.background = c(etch::Color::transparent());
    s.text_color = c(text_body);
    s.muted_color = c(text_muted);
    s.heading_color = c(text_primary);
    s.link_color = c(accent);
    s.code_bg = c(bg_code);
    s.code_border = c(border_subtle);
    s.quote_bg = c(selected_subtle);
    s.quote_border = c(accent_border);
    s.table_header_bg = c(bg_elevated);
    s.table_border = c(border_subtle);
    s.highlight_bg = c(accent_soft);
    s.selection_color = c(accent_soft);
    s.font_id = font_body;
    s.bold_font_id = font_bold;
    s.mono_font_id = font_mono;
    s.line_height_scale = 1.58f;
    s.heading_scale[0] = 1.44f;
    s.heading_scale[1] = 1.24f;
    s.heading_scale[2] = 1.12f;
    s.heading_scale[3] = 1.04f;
    s.heading_scale[4] = 1.00f;
    s.heading_scale[5] = 0.96f;
    s.code_font_scale = 0.89f;

    // Upgraded Inlay v0.11 decoupled inline code & semantic surface tokens
    const etch::Color ic_bg = dark ? etch::Color::rgba8(244, 238, 226, 16) : etch::Color::rgba8(28, 25, 21, 13);
    s.inline_code_bg = c(ic_bg);
    s.inline_code_color = c(text_primary);
    s.inline_code_no_border = true;
    s.inline_code_padding_x = 5.0f;
    s.inline_code_padding_y = 0.0f;
    s.inline_code_radius_px = 4.0f;
    s.code_color = c(text_body);
    s.code_header_bg = c(bg_surface);
    s.alert_colors[0] = c(secondary); // NOTE
    s.alert_colors[1] = c(success);   // TIP
    s.alert_colors[2] = c(accent);    // IMPORTANT
    s.alert_colors[3] = c(warning);   // WARNING
    s.alert_colors[4] = c(danger);    // CAUTION
    s.alert_bg_alpha = dark ? 0.08f : 0.07f;
    s.table_bg = c(bg_surface);
    s.table_zebra_bg = c(dark ? etch::Color::rgba8(244, 238, 226, 7) : etch::Color::rgba8(28, 25, 21, 6));
    s.table_header_color = c(text_primary);
    return s;
}

namespace {

std::string to_lower_ascii(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    return out;
}

bool system_font_installed(std::string_view family) {
    if (family.empty()) return false;
#ifdef _WIN32
    static std::vector<std::string> cached_names;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        const HKEY roots[2] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
        for (HKEY root : roots) {
            HKEY key = nullptr;
            if (RegOpenKeyExW(root, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &key) != ERROR_SUCCESS) {
                continue;
            }
            for (DWORD idx = 0; ; ++idx) {
                wchar_t val_name[256] = {};
                DWORD val_len = 256;
                wchar_t data[512] = {};
                DWORD data_bytes = sizeof(data) - sizeof(wchar_t);
                DWORD type = 0;
                const LONG rc = RegEnumValueW(key, idx, val_name, &val_len, nullptr, &type,
                                              reinterpret_cast<LPBYTE>(data), &data_bytes);
                if (rc == ERROR_NO_MORE_ITEMS) break;
                if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || data_bytes < 4) continue;
                int n = WideCharToMultiByte(CP_UTF8, 0, val_name, -1, nullptr, 0, nullptr, nullptr);
                if (n > 1) {
                    std::string utf8(static_cast<size_t>(n - 1), '\0');
                    WideCharToMultiByte(CP_UTF8, 0, val_name, -1, utf8.data(), n, nullptr, nullptr);
                    cached_names.push_back(to_lower_ascii(utf8));
                }
            }
            RegCloseKey(key);
        }
    }
    const std::string needle = to_lower_ascii(family);
    for (const auto& entry : cached_names) {
        if (entry.find(needle) != std::string::npos) return true;
    }
    return false;
#else
    namespace fs = std::filesystem;
    static std::vector<std::string> cached_files;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        const char* dirs[] = {
#ifdef __APPLE__
            "/System/Library/Fonts",
            "/Library/Fonts",
#else
            "/usr/share/fonts",
            "/usr/local/share/fonts",
#endif
        };
        std::error_code ec;
        for (const char* d : dirs) {
            if (!fs::is_directory(d, ec)) continue;
            for (const auto& e : fs::recursive_directory_iterator(d, fs::directory_options::skip_permission_denied, ec)) {
                if (ec) { ec.clear(); continue; }
                if (!e.is_regular_file(ec)) continue;
                cached_files.push_back(to_lower_ascii(e.path().filename().string()));
            }
        }
    }
    std::string compact;
    for (char c : family) {
        if (c != ' ' && c != '-' && c != '_') compact.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    for (const auto& fn : cached_files) {
        std::string fcmp;
        for (char c : fn) {
            if (c != ' ' && c != '-' && c != '_') fcmp.push_back(c);
        }
        if (fcmp.find(compact) != std::string::npos) return true;
    }
    return false;
#endif
}

} // namespace

std::vector<FontChoice> discover_ui_fonts() {
    std::vector<FontChoice> out;
    out.push_back({"", "Geist (Default)", true});
    out.push_back({"harmonyos", "HarmonyOS Sans SC", true});

    struct Candidate { const char* family; const char* label; };
    static constexpr Candidate kCandidates[] = {
        {"Segoe UI Variable", "Segoe UI Variable"},
        {"Segoe UI",          "Segoe UI"},
        {"Microsoft YaHei UI","Microsoft YaHei UI"},
        {"PingFang SC",       "PingFang SC"},
        {"Inter",             "Inter"},
        {"MiSans",            "MiSans"},
        {"Noto Sans SC",      "Noto Sans SC"},
        {"Source Han Sans",   "Source Han Sans"},
        {"Sarasa UI SC",      "Sarasa UI SC"},
        {"LXGW WenKai",       "LXGW WenKai"},
        {"Roboto",            "Roboto"},
        {"Georgia",           "Georgia"},
    };
    for (const auto& c : kCandidates) {
        if (system_font_installed(c.family)) {
            out.push_back({c.family, c.label, false});
        }
    }
    return out;
}

std::vector<FontChoice> discover_mono_fonts() {
    std::vector<FontChoice> out;
    out.push_back({"", "Geist Mono", true});

    struct Candidate { const char* family; const char* label; };
    static constexpr Candidate kCandidates[] = {
        {"Cascadia Code",   "Cascadia Code"},
        {"Cascadia Mono",   "Cascadia Mono"},
        {"JetBrains Mono",  "JetBrains Mono"},
        {"Fira Code",       "Fira Code"},
        {"Consolas",        "Consolas"},
        {"SF Mono",         "SF Mono"},
        {"Menlo",           "Menlo"},
        {"Sarasa Mono SC",  "Sarasa Mono SC"},
        {"Maple Mono",      "Maple Mono"},
        {"DejaVu Sans Mono","DejaVu Sans Mono"},
    };
    for (const auto& c : kCandidates) {
        if (system_font_installed(c.family)) {
            out.push_back({c.family, c.label, false});
        }
    }
    return out;
}

FontSlots load_app_fonts(etch::Context* ctx,
                         std::string_view ui_font_pref,
                         std::string_view mono_font_pref,
                         const FontSlots* reuse_slots) noexcept {
    namespace fs = std::filesystem;
    FontSlots f{};
    const uint16_t hint_ui_tnum    = (reuse_slots && reuse_slots->ui_tnum != etch::kFontMedium && reuse_slots->ui_tnum != etch::kFontSans) ? reuse_slots->ui_tnum : etch::kInvalidFont;
    const uint16_t hint_mono_tnum  = (reuse_slots && reuse_slots->mono_tnum != etch::kFontMono) ? reuse_slots->mono_tnum : etch::kInvalidFont;
    const uint16_t hint_mono_med   = (reuse_slots && reuse_slots->mono_medium != etch::kFontMono) ? reuse_slots->mono_medium : etch::kInvalidFont;
    const uint16_t hint_serif      = (reuse_slots && reuse_slots->serif != etch::kFontBold && reuse_slots->serif != etch::kFontSemiBold) ? reuse_slots->serif : etch::kInvalidFont;
    const uint16_t hint_cjk_reg    = (reuse_slots && reuse_slots->cjk_regular != etch::kFontSans) ? reuse_slots->cjk_regular : etch::kInvalidFont;

    auto resolve_font_dir = []() -> fs::path {
        std::error_code ec;
        if (const char* base = SDL_GetBasePath()) {
            fs::path p = fs::path(base) / "assets" / "fonts";
            if (fs::is_directory(p, ec)) return p;
        }
        {
            fs::path p = fs::path("assets") / "fonts";
            if (fs::is_directory(p, ec)) return p;
        }
#ifdef OMPE_SOURCE_DIR
        {
            fs::path p = fs::path(OMPE_SOURCE_DIR) / "assets" / "fonts";
            if (fs::is_directory(p, ec)) return p;
        }
#endif
        return {};
    };

    const fs::path font_dir = resolve_font_dir();
    auto load_bundled = [&](const char* filename, uint16_t slot_hint, float scale_adjust) -> uint16_t {
        if (font_dir.empty()) return etch::kInvalidFont;
        std::error_code ec;
        const fs::path full = font_dir / filename;
        if (!fs::is_regular_file(full, ec)) return etch::kInvalidFont;
        const std::string utf8 = full.string();
        const uint16_t slot = etch::load_font_file(ctx, utf8.c_str(), slot_hint);
        if (slot != etch::kInvalidFont) {
            etch::set_font_scale_adjust(ctx, slot, scale_adjust);
        }
        return slot;
    };

    constexpr float kGeistCssScale   = 1.30f;
    constexpr float kHarmonyCssScale = 1.125f;
    constexpr float kSysSansScale    = 1.16f;
    constexpr float kSysMonoScale    = 1.14f;

    // 1. UI & Reading Font
    std::string ui_label;
    const bool want_bundled_default = ui_font_pref.empty() || ui_font_pref == "auto";
    const bool want_harmony_only    = (ui_font_pref == "harmonyos");
    bool ui_loaded = false;

    if (!want_bundled_default && !want_harmony_only) {
        const etch::Str fam{ui_font_pref.data(), static_cast<uint32_t>(ui_font_pref.size()), false};
        const uint16_t reg = etch::load_system_font(ctx, fam, etch::FontWeight::Regular, etch::kFontSans);
        if (reg != etch::kInvalidFont) {
            etch::set_font_scale_adjust(ctx, etch::kFontSans, kSysSansScale);
            if (etch::load_system_font(ctx, fam, etch::FontWeight::Medium, etch::kFontMedium) == etch::kInvalidFont) {
                etch::load_system_font(ctx, fam, etch::FontWeight::Regular, etch::kFontMedium);
            }
            etch::set_font_scale_adjust(ctx, etch::kFontMedium, kSysSansScale);
            if (etch::load_system_font(ctx, fam, etch::FontWeight::SemiBold, etch::kFontSemiBold) == etch::kInvalidFont) {
                etch::load_system_font(ctx, fam, etch::FontWeight::Bold, etch::kFontSemiBold);
            }
            etch::set_font_scale_adjust(ctx, etch::kFontSemiBold, kSysSansScale);
            if (etch::load_system_font(ctx, fam, etch::FontWeight::Bold, etch::kFontBold) == etch::kInvalidFont) {
                etch::load_system_font(ctx, fam, etch::FontWeight::Regular, etch::kFontBold);
            }
            etch::set_font_scale_adjust(ctx, etch::kFontBold, kSysSansScale);
            if (etch::load_system_font(ctx, fam, etch::FontWeight::Italic, etch::kFontItalic) == etch::kInvalidFont) {
                etch::load_system_font(ctx, fam, etch::FontWeight::Regular, etch::kFontItalic);
            }
            etch::set_font_scale_adjust(ctx, etch::kFontItalic, kSysSansScale);

            const uint16_t ui_t = etch::load_system_font(ctx, fam, etch::FontWeight::Medium, hint_ui_tnum);
            if (ui_t != etch::kInvalidFont) {
                etch::set_font_scale_adjust(ctx, ui_t, kSysSansScale);
                etch::set_font_tabular_numbers(ctx, ui_t, true);
                f.ui_tnum = ui_t;
            }
            ui_label = std::string(ui_font_pref);
            ui_loaded = true;
        } else {
            f.ui_pref_matched = false;
        }
    }

    if (!ui_loaded && want_harmony_only) {
        if (load_bundled("HarmonyOS_Sans_SC_Regular.ttf", etch::kFontSans, kHarmonyCssScale) != etch::kInvalidFont) {
            ui_label = "HarmonyOS Sans SC";
            ui_loaded = true;
        }
    }

    if (!ui_loaded) {
        const bool has_geist = (load_bundled("Geist-Regular.ttf", etch::kFontSans, kGeistCssScale) != etch::kInvalidFont);
        if (has_geist) {
            load_bundled("Geist-Medium.ttf",   etch::kFontMedium,   kGeistCssScale);
            load_bundled("Geist-SemiBold.ttf", etch::kFontSemiBold, kGeistCssScale);
            load_bundled("Geist-Bold.ttf",     etch::kFontBold,     kGeistCssScale);
            load_bundled("Geist-Italic.ttf",   etch::kFontItalic,   kGeistCssScale);
            const uint16_t ui_t = load_bundled("Geist-Medium.ttf", hint_ui_tnum, kGeistCssScale);
            if (ui_t != etch::kInvalidFont) {
                etch::set_font_tabular_numbers(ctx, ui_t, true);
                f.ui_tnum = ui_t;
            }
            ui_label = "Geist";
        } else {
            etch::set_font_scale_adjust(ctx, etch::kFontSans, 1.24f);
            etch::set_font_scale_adjust(ctx, etch::kFontBold, 1.24f);
            ui_label = "System UI";
        }
    }

    // 2. Monospace / Code Font
    std::string mono_label;
    bool mono_loaded = false;
    if (!mono_font_pref.empty() && mono_font_pref != "auto") {
        const etch::Str mfam{mono_font_pref.data(), static_cast<uint32_t>(mono_font_pref.size()), false};
        uint16_t mslot = etch::load_system_font(ctx, mfam, etch::FontWeight::Mono, etch::kFontMono);
        if (mslot == etch::kInvalidFont) {
            mslot = etch::load_system_font(ctx, mfam, etch::FontWeight::Regular, etch::kFontMono);
        }
        if (mslot != etch::kInvalidFont) {
            etch::set_font_scale_adjust(ctx, etch::kFontMono, kSysMonoScale);
            f.mono = etch::kFontMono;
            uint16_t mt = etch::load_system_font(ctx, mfam, etch::FontWeight::Mono, hint_mono_tnum);
            if (mt == etch::kInvalidFont) mt = etch::load_system_font(ctx, mfam, etch::FontWeight::Regular, hint_mono_tnum);
            if (mt != etch::kInvalidFont) {
                etch::set_font_scale_adjust(ctx, mt, kSysMonoScale);
                etch::set_font_tabular_numbers(ctx, mt, true);
                f.mono_tnum = mt;
            }
            uint16_t mm = etch::load_system_font(ctx, mfam, etch::FontWeight::Medium, hint_mono_med);
            if (mm != etch::kInvalidFont) {
                etch::set_font_scale_adjust(ctx, mm, kSysMonoScale);
                f.mono_medium = mm;
            } else {
                f.mono_medium = f.mono;
            }
            mono_label = std::string(mono_font_pref);
            mono_loaded = true;
        } else {
            f.mono_pref_matched = false;
        }
    }

    if (!mono_loaded) {
        const bool has_geist_mono = (load_bundled("GeistMono-Regular.ttf", etch::kFontMono, kGeistCssScale) != etch::kInvalidFont);
        if (has_geist_mono) {
            f.mono = etch::kFontMono;
            const uint16_t mt = load_bundled("GeistMono-Regular.ttf", hint_mono_tnum, kGeistCssScale);
            if (mt != etch::kInvalidFont) {
                etch::set_font_tabular_numbers(ctx, mt, true);
                f.mono_tnum = mt;
            }
            const uint16_t mm = load_bundled("GeistMono-Medium.ttf", hint_mono_med, kGeistCssScale);
            if (mm != etch::kInvalidFont) {
                f.mono_medium = mm;
            }
            mono_label = "Geist Mono";
        } else {
            const char* mono_candidates[] = {
#ifdef _WIN32
                "C:/Windows/Fonts/CascadiaMono.ttf",
                "C:/Windows/Fonts/CascadiaCode.ttf",
                "C:/Windows/Fonts/consola.ttf",
#elif defined(__APPLE__)
                "/System/Library/Fonts/SFNSMono.ttf",
                "/System/Library/Fonts/Menlo.ttc",
#else
                "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf",
                "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
#endif
            };
            for (const char* path : mono_candidates) {
                const uint16_t slot = etch::load_font_file(ctx, path, etch::kFontMono);
                if (slot != etch::kInvalidFont) {
                    etch::set_font_scale_adjust(ctx, slot, 1.18f);
                    f.mono = slot;
                    break;
                }
            }
            for (const char* path : mono_candidates) {
                const uint16_t slot = etch::load_font_file(ctx, path, hint_mono_tnum);
                if (slot != etch::kInvalidFont) {
                    etch::set_font_scale_adjust(ctx, slot, 1.18f);
                    etch::set_font_tabular_numbers(ctx, slot, true);
                    f.mono_tnum = slot;
                    break;
                }
            }
            if (f.mono_tnum == etch::kFontMono) {
                etch::set_font_tabular_numbers(ctx, etch::kFontMono, true);
            }
            f.mono_medium = f.mono;
            mono_label = "Monospace";
        }
    }

    // 3. Editorial high-contrast display serif for OMP brand wordmark (Instrument Serif)
    const uint16_t serif_slot = load_bundled("InstrumentSerif-Regular.ttf", hint_serif, kGeistCssScale);
    if (serif_slot != etch::kInvalidFont) {
        f.serif = serif_slot;
    }

    // 4. Weight-matched CJK companion fallback:
    //    Prefer zero-extra-RAM OS-shared CJK fonts on Windows/macOS (Microsoft YaHei UI / PingFang SC) so
    //    startup memory stays lean (<100 MB), and only map bundled HarmonyOS Sans SC on Linux or when selected.
    if (want_harmony_only) {
        f.cjk_regular = etch::kFontSans;
        f.cjk_medium  = etch::kFontMedium;
        f.cjk_bold    = etch::kFontBold;
    } else {
        const char* os_cjk_family =
#ifdef _WIN32
            "Microsoft YaHei UI";
#elif defined(__APPLE__)
            "PingFang SC";
#else
            nullptr;
#endif
        if (os_cjk_family) {
            const etch::Str cjk_fam = etch::str_cstr(os_cjk_family);
            f.cjk_regular = etch::load_system_font(ctx, cjk_fam, etch::FontWeight::Regular, hint_cjk_reg);
            if (f.cjk_regular != etch::kInvalidFont) {
                etch::set_font_scale_adjust(ctx, f.cjk_regular, 1.12f);
                // Reuse the single OS-shared CJK Regular face across weights so we don't map a second 32 MB Bold TTC
                f.cjk_medium = f.cjk_regular;
                f.cjk_bold   = f.cjk_regular;
            }
        }
        if (f.cjk_regular == etch::kInvalidFont) {
            f.cjk_regular = load_bundled("HarmonyOS_Sans_SC_Regular.ttf", hint_cjk_reg, kHarmonyCssScale);
            f.cjk_medium  = f.cjk_regular;
            f.cjk_bold    = f.cjk_regular;
        }
    }

    if (f.cjk_regular != etch::kInvalidFont && !want_harmony_only) {
        const uint16_t cjk_med = (f.cjk_medium != etch::kInvalidFont) ? f.cjk_medium : f.cjk_regular;
        const uint16_t cjk_bld = (f.cjk_bold != etch::kInvalidFont) ? f.cjk_bold : cjk_med;

        etch::set_font_fallback(ctx, etch::kFontSans,     f.cjk_regular);
        etch::set_font_fallback(ctx, etch::kFontItalic,   f.cjk_regular);
        etch::set_font_fallback(ctx, etch::kFontMono,     f.cjk_regular);
        if (f.mono_tnum != etch::kFontMono)   etch::set_font_fallback(ctx, f.mono_tnum,   f.cjk_regular);
        if (f.mono_medium != etch::kFontMono) etch::set_font_fallback(ctx, f.mono_medium, cjk_med);
        etch::set_font_fallback(ctx, etch::kFontMedium,   cjk_med);
        etch::set_font_fallback(ctx, etch::kFontSemiBold, cjk_med);
        if (f.ui_tnum != etch::kFontMedium)   etch::set_font_fallback(ctx, f.ui_tnum,     cjk_med);
        if (serif_slot != etch::kInvalidFont) etch::set_font_fallback(ctx, serif_slot,    f.cjk_regular);
        etch::set_font_fallback(ctx, etch::kFontBold,     cjk_bld);
    }

    if (want_harmony_only && ui_loaded) {
        f.body     = etch::kFontSans;
        f.display  = etch::kFontSans;
        f.medium   = etch::kFontSans;
        f.semibold = etch::kFontSans;
        f.bold     = etch::kFontSans;
        f.italic   = etch::kFontSans;
        f.ui_tnum  = etch::kFontSans;
        f.cjk_regular = etch::kFontSans;
        f.cjk_medium  = etch::kFontSans;
        f.cjk_bold    = etch::kFontSans;
    } else {
        f.display  = etch::is_font_loaded(ctx, etch::kFontSemiBold) ? etch::kFontSemiBold : etch::kFontBold;
        f.medium   = etch::is_font_loaded(ctx, etch::kFontMedium)   ? etch::kFontMedium   : etch::kFontSans;
        f.semibold = etch::is_font_loaded(ctx, etch::kFontSemiBold) ? etch::kFontSemiBold : etch::kFontBold;
        if (f.ui_tnum == etch::kFontMedium) {
            f.ui_tnum = f.medium;
            etch::set_font_tabular_numbers(ctx, f.ui_tnum, true);
        }
    }
    if (serif_slot == etch::kInvalidFont) {
        f.serif = f.display;
    }

    f.family_label = ui_label + " · " + mono_label;
    etch::set_hinted_small_text(ctx, true);
    return f;
}

} // namespace ompe
