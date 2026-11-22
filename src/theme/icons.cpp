#include "theme/icons.h"

namespace ompe {
namespace {

uint16_t stroke_icon(etch::Context* ctx, const char* d, float sw = 1.75f) noexcept {
    return etch::register_svg_stroke_icon(ctx, etch::str_cstr(d), etch::SvgStrokeDesc{
        .view_box = {0.0f, 0.0f, 24.0f, 24.0f},
        .stroke_width = sw,
        .cap = etch::StrokeCap::Round,
        .join = etch::StrokeJoin::Round,
    });
}

uint16_t duotone_icon(etch::Context* ctx, const char* fill_d, const char* stroke_d,
                      float sw = 1.75f, float fill_alpha = 0.20f) noexcept {
    return etch::register_svg_duotone_icon(ctx, etch::str_cstr(fill_d), etch::str_cstr(stroke_d), etch::SvgStrokeDesc{
        .view_box = {0.0f, 0.0f, 24.0f, 24.0f},
        .stroke_width = sw,
        .cap = etch::StrokeCap::Round,
        .join = etch::StrokeJoin::Round,
    }, fill_alpha);
}

// Reusable Lucide rounded 18x18 frame path: x=3..21, y=3..21, rx=2.5
constexpr const char* kRoundedFrame18 =
    "M5.5 3 H18.5 A2.5 2.5 0 0 1 21 5.5 V18.5 A2.5 2.5 0 0 1 18.5 21 H5.5 A2.5 2.5 0 0 1 3 18.5 V5.5 A2.5 2.5 0 0 1 5.5 3 Z";

} // namespace

AppIcons register_app_icons(etch::Context* ctx) noexcept {
    AppIcons i{};

    // Sculpted architectural duotone astrolabe crest (replaces generic boxed Pi — pairs with Instrument Serif OMP wordmark)
    i.logo = duotone_icon(ctx,
        "M12 4.6 C12.5 8.8 15.2 11.5 19.4 12 C15.2 12.5 12.5 15.2 12 19.4 C11.5 15.2 8.8 12.5 4.6 12 C8.8 11.5 11.5 8.8 12 4.6 Z",
        "M12 2.6 L21.4 12 L12 21.4 L2.6 12 Z M12 5.4 C12.5 9.1 14.9 11.5 18.6 12 C14.9 12.5 12.5 14.9 12 18.6 C11.5 14.9 9.1 12.5 5.4 12 C9.1 11.5 11.5 9.1 12 5.4 Z",
        1.45f, 0.34f);

    // State-reactive Sidebar & Workbench panel icons (Lucide panel-left/right-open/close with subtle duotone rail)
    i.panel_left_close = duotone_icon(ctx,
        "M5.5 3 H9 V21 H5.5 A2.5 2.5 0 0 1 3 18.5 V5.5 A2.5 2.5 0 0 1 5.5 3 Z",
        "M5.5 3 H18.5 A2.5 2.5 0 0 1 21 5.5 V18.5 A2.5 2.5 0 0 1 18.5 21 H5.5 A2.5 2.5 0 0 1 3 18.5 V5.5 A2.5 2.5 0 0 1 5.5 3 Z M9 3 V21 M16 15 L13 12 L16 9",
        1.7f, 0.18f);
    i.panel_left_open = stroke_icon(ctx,
        "M5.5 3 H18.5 A2.5 2.5 0 0 1 21 5.5 V18.5 A2.5 2.5 0 0 1 18.5 21 H5.5 A2.5 2.5 0 0 1 3 18.5 V5.5 A2.5 2.5 0 0 1 5.5 3 Z M9 3 V21 M14 9 L17 12 L14 15",
        1.7f);
    i.panel_right_close = duotone_icon(ctx,
        "M15 3 H18.5 A2.5 2.5 0 0 1 21 5.5 V18.5 A2.5 2.5 0 0 1 18.5 21 H15 Z",
        "M5.5 3 H18.5 A2.5 2.5 0 0 1 21 5.5 V18.5 A2.5 2.5 0 0 1 18.5 21 H5.5 A2.5 2.5 0 0 1 3 18.5 V5.5 A2.5 2.5 0 0 1 5.5 3 Z M15 3 V21 M8 9 L11 12 L8 15",
        1.7f, 0.18f);
    i.panel_right_open = stroke_icon(ctx,
        "M5.5 3 H18.5 A2.5 2.5 0 0 1 21 5.5 V18.5 A2.5 2.5 0 0 1 18.5 21 H5.5 A2.5 2.5 0 0 1 3 18.5 V5.5 A2.5 2.5 0 0 1 5.5 3 Z M15 3 V21 M10 15 L7 12 L10 9",
        1.7f);

    // Navigation & Workspace
    i.plus = stroke_icon(ctx, "M12 5 V19 M5 12 H19", 1.85f);
    i.search = stroke_icon(ctx, "M11 4 A7 7 0 1 0 11 18 A7 7 0 1 0 11 4 M20 20 L16.2 16.2", 1.75f);
    i.folder = stroke_icon(ctx,
        "M4 6 C4 4.9 4.9 4 6 4 H9.3 C9.9 4 10.5 4.3 10.9 4.8 L12.2 6.5 C12.6 7 13.2 7.3 13.8 7.3 H18.5 C19.9 7.3 21 8.4 21 9.8 V17.5 C21 18.9 19.9 20 18.5 20 H5.5 C4.1 20 3 18.9 3 17.5 V6 Z",
        1.7f);
    i.folder_open = duotone_icon(ctx,
        "M3.5 11.5 H20.5 L19.2 18.5 C19 19.4 18.2 20 17.3 20 H6.7 C5.8 20 5 19.4 4.8 18.5 Z",
        "M5 19.5 H17.8 C18.8 19.5 19.6 18.8 19.8 17.8 L21.2 11.8 C21.4 10.8 20.6 9.8 19.5 9.8 H6.8 C5.8 9.8 5 10.5 4.8 11.5 L3.4 17.5 C3.1 18.6 3.9 19.5 5 19.5 Z M3.2 16.5 V6.5 C3.2 5.4 4.1 4.5 5.2 4.5 H9.1 C9.8 4.5 10.4 4.8 10.8 5.4 L11.8 6.8 C12.2 7.3 12.8 7.6 13.4 7.6 H17.8 C18.9 7.6 19.8 8.5 19.8 9.6",
        1.7f, 0.22f);
    i.pin = stroke_icon(ctx,
        "M12 17 V21 M9 10.8 A2 2 0 0 1 7.9 12.6 L6.3 13.5 A2 2 0 0 0 5.3 15.2 V16 A1 1 0 0 0 6.3 17 H17.7 A1 1 0 0 0 18.7 16 V15.2 A2 2 0 0 0 17.7 13.5 L16.1 12.6 A2 2 0 0 1 15 10.8 V5 A1 1 0 0 1 16 4 A1 1 0 0 0 16 2 H8 A1 1 0 0 0 8 4 A1 1 0 0 1 9 5 Z",
        1.7f);
    i.pin_filled = duotone_icon(ctx,
        "M9 5 H15 V10.8 A2 2 0 0 0 16.1 12.6 L17.7 13.5 A2 2 0 0 1 18.7 15.2 V16.5 H5.3 V15.2 A2 2 0 0 1 6.3 13.5 L7.9 12.6 A2 2 0 0 0 9 10.8 Z",
        "M12 17 V21 M9 10.8 A2 2 0 0 1 7.9 12.6 L6.3 13.5 A2 2 0 0 0 5.3 15.2 V16 A1 1 0 0 0 6.3 17 H17.7 A1 1 0 0 0 18.7 16 V15.2 A2 2 0 0 0 17.7 13.5 L16.1 12.6 A2 2 0 0 1 15 10.8 V5 A1 1 0 0 1 16 4 A1 1 0 0 0 16 2 H8 A1 1 0 0 0 8 4 A1 1 0 0 1 9 5 Z",
        1.7f, 0.34f);
    i.message = stroke_icon(ctx,
        "M6 4.5 H18 A2.5 2.5 0 0 1 20.5 7 V15 A2.5 2.5 0 0 1 18 17.5 H8.8 L4.5 20.8 V17.5 A2.5 2.5 0 0 1 3.5 15.5 V7 A2.5 2.5 0 0 1 6 4.5 Z",
        1.7f);

    // Disclosures & Animated Status
    i.chevron_r = stroke_icon(ctx, "M9.5 6.5 L15 12 L9.5 17.5", 1.85f);
    i.chevron_d = stroke_icon(ctx, "M6.5 9.5 L12 15 L17.5 9.5", 1.85f);
    // Clean 270-degree orbital spinner arc (animated smoothly with IconDesc::rotation_rad)
    i.loader_arc = stroke_icon(ctx, "M20.5 12 A8.5 8.5 0 1 1 12 3.5", 2.0f);
    i.check = stroke_icon(ctx, "M4.5 12.5 L9.5 17.5 L19.5 7.5", 2.0f);
    i.check_circle = duotone_icon(ctx,
        "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 Z",
        "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 M8.8 12.3 L11 14.5 L15.5 9.8",
        1.75f, 0.18f);
    i.x_circle = duotone_icon(ctx,
        "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 Z",
        "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 M9.2 9.2 L14.8 14.8 M14.8 9.2 L9.2 14.8",
        1.75f, 0.18f);
    i.alert = duotone_icon(ctx,
        "M10.3 4.2 L2.5 18 A2 2 0 0 0 4.2 21 H19.8 A2 2 0 0 0 21.5 18 L13.7 4.2 A2 2 0 0 0 10.3 4.2 Z",
        "M10.3 4.2 L2.5 18 A2 2 0 0 0 4.2 21 H19.8 A2 2 0 0 0 21.5 18 L13.7 4.2 A2 2 0 0 0 10.3 4.2 Z M12 9.5 V13.5 M12 17 Z",
        1.75f, 0.18f);

    // Composer & Voice controls
    i.send = stroke_icon(ctx, "M12 19 V5 M6.5 10.5 L12 5 L17.5 10.5", 2.0f);
    i.stop = duotone_icon(ctx,
        "M8 6.5 H16 A1.5 1.5 0 0 1 17.5 8 V16 A1.5 1.5 0 0 1 16 17.5 H8 A1.5 1.5 0 0 1 6.5 16 V8 A1.5 1.5 0 0 1 8 6.5 Z",
        "M8 6.5 H16 A1.5 1.5 0 0 1 17.5 8 V16 A1.5 1.5 0 0 1 16 17.5 H8 A1.5 1.5 0 0 1 6.5 16 V8 A1.5 1.5 0 0 1 8 6.5 Z",
        1.75f, 0.85f);
    i.steer = stroke_icon(ctx, "M9 10 L4 15 L9 20 M20 4 V11 A4 4 0 0 1 16 15 H4", 1.85f);
    i.sparkle = stroke_icon(ctx,
        "M9.94 15.5 A2 2 0 0 0 8.5 14.06 L3.1 12.66 A0.65 0.65 0 0 1 3.1 11.34 L8.5 9.94 A2 2 0 0 0 9.94 8.5 L11.34 3.1 A0.65 0.65 0 0 1 12.66 3.1 L14.06 8.5 A2 2 0 0 0 15.5 9.94 L20.9 11.34 A0.65 0.65 0 0 1 20.9 12.66 L15.5 14.06 A2 2 0 0 0 14.06 15.5 L12.66 20.9 A0.65 0.65 0 0 1 11.34 20.9 Z",
        1.65f);
    i.sparkle_active = duotone_icon(ctx,
        "M9.94 15.5 A2 2 0 0 0 8.5 14.06 L3.1 12.66 A0.65 0.65 0 0 1 3.1 11.34 L8.5 9.94 A2 2 0 0 0 9.94 8.5 L11.34 3.1 A0.65 0.65 0 0 1 12.66 3.1 L14.06 8.5 A2 2 0 0 0 15.5 9.94 L20.9 11.34 A0.65 0.65 0 0 1 20.9 12.66 L15.5 14.06 A2 2 0 0 0 14.06 15.5 L12.66 20.9 A0.65 0.65 0 0 1 11.34 20.9 Z",
        "M9.94 15.5 A2 2 0 0 0 8.5 14.06 L3.1 12.66 A0.65 0.65 0 0 1 3.1 11.34 L8.5 9.94 A2 2 0 0 0 9.94 8.5 L11.34 3.1 A0.65 0.65 0 0 1 12.66 3.1 L14.06 8.5 A2 2 0 0 0 15.5 9.94 L20.9 11.34 A0.65 0.65 0 0 1 20.9 12.66 L15.5 14.06 A2 2 0 0 0 14.06 15.5 L12.66 20.9 A0.65 0.65 0 0 1 11.34 20.9 Z M19.5 3.5 V6.5 M18 5 H21",
        1.65f, 0.28f);
    i.paperclip = stroke_icon(ctx,
        "M13.2 6.8 L7.1 12.9 A2.1 2.1 0 0 0 10.1 15.9 L17.6 8.4 A3.8 3.8 0 0 0 12.2 3 L4.7 10.5 A5.5 5.5 0 0 0 12.5 18.3 L18.8 12",
        1.7f);
    i.mic = stroke_icon(ctx,
        "M12 3 A3 3 0 0 0 9 6 V11 A3 3 0 0 0 15 11 V6 A3 3 0 0 0 12 3 Z M18.5 10.5 V11 A6.5 6.5 0 0 1 5.5 11 V10.5 M12 17.5 V21 M8.5 21 H15.5",
        1.7f);
    i.audio_lines = stroke_icon(ctx,
        "M3 10 V14 M7.5 6.5 V17.5 M12 3.5 V20.5 M16.5 7.5 V16.5 M21 10 V14",
        1.9f);

    // Semantic Tool & Workbench Icons
    i.terminal = stroke_icon(ctx,
        "M5.5 3 H18.5 A2.5 2.5 0 0 1 21 5.5 V18.5 A2.5 2.5 0 0 1 18.5 21 H5.5 A2.5 2.5 0 0 1 3 18.5 V5.5 A2.5 2.5 0 0 1 5.5 3 Z M7.5 9.5 L10.5 12 L7.5 14.5 M12.5 14.5 H16.5",
        1.7f);
    i.terminal_duotone = duotone_icon(ctx, kRoundedFrame18,
        "M5.5 3 H18.5 A2.5 2.5 0 0 1 21 5.5 V18.5 A2.5 2.5 0 0 1 18.5 21 H5.5 A2.5 2.5 0 0 1 3 18.5 V5.5 A2.5 2.5 0 0 1 5.5 3 Z M7.5 9.5 L10.5 12 L7.5 14.5 M12.5 14.5 H16.5",
        1.7f, 0.16f);
    i.code = stroke_icon(ctx, "M8 8 L3.8 12 L8 16 M16 8 L20.2 12 L16 16 M14 5 L10 19", 1.75f);
    i.file = stroke_icon(ctx,
        "M6.5 3 H14 L19.5 8.5 V19 A2 2 0 0 1 17.5 21 H6.5 A2 2 0 0 1 4.5 19 V5 A2 2 0 0 1 6.5 3 Z M14 3 V8.5 H19.5",
        1.7f);
    i.file_text = stroke_icon(ctx,
        "M6.5 3 H14 L19.5 8.5 V19 A2 2 0 0 1 17.5 21 H6.5 A2 2 0 0 1 4.5 19 V5 A2 2 0 0 1 6.5 3 Z M14 3 V8.5 H19.5 M8.5 13 H15.5 M8.5 16.5 H13.5",
        1.7f);
    i.file_diff = stroke_icon(ctx,
        "M6.5 3 H14 L19.5 8.5 V19 A2 2 0 0 1 17.5 21 H6.5 A2 2 0 0 1 4.5 19 V5 A2 2 0 0 1 6.5 3 Z M14 3 V8.5 H19.5 M9.5 12.5 H14.5 M12 10 V15 M9.5 17.5 H14.5",
        1.7f);
    i.file_diff_duotone = duotone_icon(ctx,
        "M6.5 3 H14 L19.5 8.5 V19 A2 2 0 0 1 17.5 21 H6.5 A2 2 0 0 1 4.5 19 V5 A2 2 0 0 1 6.5 3 Z",
        "M6.5 3 H14 L19.5 8.5 V19 A2 2 0 0 1 17.5 21 H6.5 A2 2 0 0 1 4.5 19 V5 A2 2 0 0 1 6.5 3 Z M14 3 V8.5 H19.5 M9.5 12.5 H14.5 M12 10 V15 M9.5 17.5 H14.5",
        1.7f, 0.16f);
    i.edit = stroke_icon(ctx,
        "M12 20 H20 M16.4 3.6 A2.1 2.1 0 0 1 19.4 6.6 L8 18 L4 19 L5 15 Z",
        1.7f);
    i.folder_search = stroke_icon(ctx,
        "M10.5 20 H5.5 A2.2 2.2 0 0 1 3.3 17.8 V6.2 A2.2 2.2 0 0 1 5.5 4 H9.3 L11.5 6.8 H18.5 A2.2 2.2 0 0 1 20.7 9 V11.2 M16.8 14 A2.8 2.8 0 1 0 16.8 19.6 A2.8 2.8 0 1 0 16.8 14 M21 21 L18.9 18.9",
        1.7f);
    i.list_checks = duotone_icon(ctx,
        "M3.5 5.5 L5.2 7.2 L8.2 4.2 M3.5 12.5 L5.2 14.2 L8.2 11.2",
        "M3.5 6.5 L5.2 8.2 L8.5 4.8 M3.5 13.5 L5.2 15.2 L8.5 11.8 M12 6.5 H20.5 M12 13.5 H20.5 M12 19.5 H18.5 M5.5 19.5 Z",
        1.75f, 0.0f);
    // Clean Lucide git-branch
    i.branch = stroke_icon(ctx,
        "M6.5 3.5 V15 M17.5 3.5 A2.8 2.8 0 1 0 17.5 9.1 A2.8 2.8 0 1 0 17.5 3.5 M6.5 15 A2.8 2.8 0 1 0 6.5 20.6 A2.8 2.8 0 1 0 6.5 15 M17.5 9.1 A8.5 8.5 0 0 1 9.3 17.8",
        1.7f);
    i.branch_duotone = duotone_icon(ctx,
        "M17.5 3.5 A2.8 2.8 0 1 0 17.5 9.1 A2.8 2.8 0 1 0 17.5 3.5 Z M6.5 15 A2.8 2.8 0 1 0 6.5 20.6 A2.8 2.8 0 1 0 6.5 15 Z",
        "M6.5 3.5 V15 M17.5 3.5 A2.8 2.8 0 1 0 17.5 9.1 A2.8 2.8 0 1 0 17.5 3.5 M6.5 15 A2.8 2.8 0 1 0 6.5 20.6 A2.8 2.8 0 1 0 6.5 15 M17.5 9.1 A8.5 8.5 0 0 1 9.3 17.8",
        1.7f, 0.25f);
    // Clean geometric multi-agent constellation (Bot/Nodes — sharp & refined, zero stick figures!)
    i.agents = stroke_icon(ctx,
        "M7.5 8 H16.5 A2.5 2.5 0 0 1 19 10.5 V16.5 A2.5 2.5 0 0 1 16.5 19 H7.5 A2.5 2.5 0 0 1 5 16.5 V10.5 A2.5 2.5 0 0 1 7.5 8 Z M12 8 V4.5 M12 4.5 Z M9.2 13.2 Z M14.8 13.2 Z M3 13.5 H5 M19 13.5 H21",
        1.7f);
    i.agents_duotone = duotone_icon(ctx,
        "M7.5 8 H16.5 A2.5 2.5 0 0 1 19 10.5 V16.5 A2.5 2.5 0 0 1 16.5 19 H7.5 A2.5 2.5 0 0 1 5 16.5 V10.5 A2.5 2.5 0 0 1 7.5 8 Z",
        "M7.5 8 H16.5 A2.5 2.5 0 0 1 19 10.5 V16.5 A2.5 2.5 0 0 1 16.5 19 H7.5 A2.5 2.5 0 0 1 5 16.5 V10.5 A2.5 2.5 0 0 1 7.5 8 Z M12 8 V4.5 M12 4.5 Z M9.2 13.2 Z M14.8 13.2 Z M3 13.5 H5 M19 13.5 H21",
        1.7f, 0.20f);
    i.btw = stroke_icon(ctx,
        "M14 9 A2 2 0 0 1 12 11 H6.5 L3.5 14 V5.5 A2 2 0 0 1 5.5 3.5 H12 A2 2 0 0 1 14 5.5 Z M17.5 9.5 H18.5 A2 2 0 0 1 20.5 11.5 V20 L17.5 17 H12 A2 2 0 0 1 10 15 V14.2",
        1.7f);
    i.btw_duotone = duotone_icon(ctx,
        "M5.5 3.5 H12 A2 2 0 0 1 14 5.5 V9 A2 2 0 0 1 12 11 H6.5 L3.5 14 V5.5 A2 2 0 0 1 5.5 3.5 Z",
        "M14 9 A2 2 0 0 1 12 11 H6.5 L3.5 14 V5.5 A2 2 0 0 1 5.5 3.5 H12 A2 2 0 0 1 14 5.5 Z M17.5 9.5 H18.5 A2 2 0 0 1 20.5 11.5 V20 L17.5 17 H12 A2 2 0 0 1 10 15 V14.2",
        1.7f, 0.20f);
    i.maximize = stroke_icon(ctx, "M15 4 H20 V9 M9 20 H4 V15 M20 4 L13.8 10.2 M4 20 L10.2 13.8", 1.75f);

    // Settings Navigation (outline + active duotone pairs)
    i.settings = stroke_icon(ctx,
        "M12 9 A3 3 0 1 0 12 15 A3 3 0 1 0 12 9 M19.4 15 A1.65 1.65 0 0 0 19.73 16.82 L19.79 16.88 A2 2 0 1 1 16.96 19.71 L16.9 19.65 A1.65 1.65 0 0 0 15.08 19.32 A1.65 1.65 0 0 0 14.08 20.83 V21 A2 2 0 1 1 10.08 21 V20.91 A1.65 1.65 0 0 0 9 19.4 A1.65 1.65 0 0 0 7.18 19.73 L7.12 19.79 A2 2 0 1 1 4.29 16.96 L4.35 16.9 A1.65 1.65 0 0 0 4.68 15.08 A1.65 1.65 0 0 0 3.17 14.08 H3 A2 2 0 1 1 3 10.08 H3.09 A1.65 1.65 0 0 0 4.6 9 A1.65 1.65 0 0 0 4.27 7.18 L4.21 7.12 A2 2 0 1 1 7.04 4.29 L7.1 4.35 A1.65 1.65 0 0 0 8.92 4.68 H9 A1.65 1.65 0 0 0 10 3.17 V3 A2 2 0 1 1 14 3 V3.09 A1.65 1.65 0 0 0 15 4.6 A1.65 1.65 0 0 0 16.82 4.27 L16.88 4.21 A2 2 0 1 1 19.71 7.04 L19.65 7.1 A1.65 1.65 0 0 0 19.32 8.92 V9 A1.65 1.65 0 0 0 20.83 10 H21 A2 2 0 1 1 21 14 H20.91 A1.65 1.65 0 0 0 19.4 15 Z",
        1.6f);
    i.sliders = stroke_icon(ctx,
        "M21 5 H14 M10 5 H3 M21 12 H12 M8 12 H3 M21 19 H16 M12 19 H3 M14 3 V7 M8 10 V14 M16 17 V21",
        1.75f);
    i.sliders_active = duotone_icon(ctx,
        "M12 3.2 A1.8 1.8 0 1 0 12 6.8 A1.8 1.8 0 1 0 12 3.2 Z M10 10.2 A1.8 1.8 0 1 0 10 13.8 A1.8 1.8 0 1 0 10 10.2 Z M14 17.2 A1.8 1.8 0 1 0 14 20.8 A1.8 1.8 0 1 0 14 17.2 Z",
        "M21 5 H14 M10 5 H3 M21 12 H12 M8 12 H3 M21 19 H16 M12 19 H3 M14 3 V7 M8 10 V14 M16 17 V21",
        1.85f, 0.28f);
    i.key = stroke_icon(ctx,
        "M8 10 A4.5 4.5 0 1 0 8 19 A4.5 4.5 0 1 0 8 10 M11.4 11.4 L19.5 3.5 M16.5 6.5 L19 9 M19.5 3.5 L21.2 5.2",
        1.7f);
    i.key_active = duotone_icon(ctx,
        "M8 10 A4.5 4.5 0 1 0 8 19 A4.5 4.5 0 1 0 8 10 Z",
        "M8 10 A4.5 4.5 0 1 0 8 19 A4.5 4.5 0 1 0 8 10 M11.4 11.4 L19.5 3.5 M16.5 6.5 L19 9 M19.5 3.5 L21.2 5.2",
        1.75f, 0.24f);
    i.cpu = stroke_icon(ctx,
        "M7.5 6 H16.5 A1.5 1.5 0 0 1 18 7.5 V16.5 A1.5 1.5 0 0 1 16.5 18 H7.5 A1.5 1.5 0 0 1 6 16.5 V7.5 A1.5 1.5 0 0 1 7.5 6 Z M9.8 9.8 H14.2 V14.2 H9.8 Z M9.5 3 V6 M14.5 3 V6 M9.5 18 V21 M14.5 18 V21 M3 9.5 H6 M3 14.5 H6 M18 9.5 H21 M18 14.5 H21",
        1.65f);
    i.cpu_active = duotone_icon(ctx,
        "M7.5 6 H16.5 A1.5 1.5 0 0 1 18 7.5 V16.5 A1.5 1.5 0 0 1 16.5 18 H7.5 A1.5 1.5 0 0 1 6 16.5 V7.5 A1.5 1.5 0 0 1 7.5 6 Z",
        "M7.5 6 H16.5 A1.5 1.5 0 0 1 18 7.5 V16.5 A1.5 1.5 0 0 1 16.5 18 H7.5 A1.5 1.5 0 0 1 6 16.5 V7.5 A1.5 1.5 0 0 1 7.5 6 Z M9.8 9.8 H14.2 V14.2 H9.8 Z M9.5 3 V6 M14.5 3 V6 M9.5 18 V21 M14.5 18 V21 M3 9.5 H6 M3 14.5 H6 M18 9.5 H21 M18 14.5 H21",
        1.7f, 0.22f);
    i.palette = stroke_icon(ctx,
        "M12 3 A9 9 0 0 0 3 12 A9 9 0 0 0 12 21 A2.2 2.2 0 0 0 14.2 18.8 C14.2 18.2 14 17.7 13.7 17.3 C13.2 16.7 13.6 15.8 14.4 15.8 H16.5 A4.5 4.5 0 0 0 21 11.3 C21 6.7 17 3 12 3 Z M7.5 11.2 Z M10.2 7.6 Z M14.8 7.8 Z M17.2 11.2 Z",
        1.7f);
    i.palette_active = duotone_icon(ctx,
        "M12 3 A9 9 0 0 0 3 12 A9 9 0 0 0 12 21 A2.2 2.2 0 0 0 14.2 18.8 C14.2 18.2 14 17.7 13.7 17.3 C13.2 16.7 13.6 15.8 14.4 15.8 H16.5 A4.5 4.5 0 0 0 21 11.3 C21 6.7 17 3 12 3 Z",
        "M12 3 A9 9 0 0 0 3 12 A9 9 0 0 0 12 21 A2.2 2.2 0 0 0 14.2 18.8 C14.2 18.2 14 17.7 13.7 17.3 C13.2 16.7 13.6 15.8 14.4 15.8 H16.5 A4.5 4.5 0 0 0 21 11.3 C21 6.7 17 3 12 3 Z M7.5 11.2 Z M10.2 7.6 Z M14.8 7.8 Z M17.2 11.2 Z",
        1.75f, 0.20f);
    i.shield = stroke_icon(ctx,
        "M12 3 L19 6 V11.5 C19 16.2 16 19.6 12 21 C8 19.6 5 16.2 5 11.5 V6 Z M9.2 12.2 L11.2 14.2 L15 10.2",
        1.7f);
    i.shield_active = duotone_icon(ctx,
        "M12 3 L19 6 V11.5 C19 16.2 16 19.6 12 21 C8 19.6 5 16.2 5 11.5 V6 Z",
        "M12 3 L19 6 V11.5 C19 16.2 16 19.6 12 21 C8 19.6 5 16.2 5 11.5 V6 Z M9.2 12.2 L11.2 14.2 L15 10.2",
        1.75f, 0.22f);
    i.blocks = stroke_icon(ctx,
        "M15 3 H19.5 A1.5 1.5 0 0 1 21 4.5 V9 A1.5 1.5 0 0 1 19.5 10.5 H15 A1.5 1.5 0 0 1 13.5 9 V4.5 A1.5 1.5 0 0 1 15 3 Z M5 10.5 H12 A1.5 1.5 0 0 1 13.5 12 V19 A1.5 1.5 0 0 1 12 20.5 H5 A1.5 1.5 0 0 1 3.5 19 V12 A1.5 1.5 0 0 1 5 10.5 Z M13.5 14 H17 A1.5 1.5 0 0 1 18.5 15.5 V19 A1.5 1.5 0 0 1 17 20.5 H12",
        1.7f);
    i.blocks_active = duotone_icon(ctx,
        "M15 3 H19.5 A1.5 1.5 0 0 1 21 4.5 V9 A1.5 1.5 0 0 1 19.5 10.5 H15 A1.5 1.5 0 0 1 13.5 9 V4.5 A1.5 1.5 0 0 1 15 3 Z",
        "M15 3 H19.5 A1.5 1.5 0 0 1 21 4.5 V9 A1.5 1.5 0 0 1 19.5 10.5 H15 A1.5 1.5 0 0 1 13.5 9 V4.5 A1.5 1.5 0 0 1 15 3 Z M5 10.5 H12 A1.5 1.5 0 0 1 13.5 12 V19 A1.5 1.5 0 0 1 12 20.5 H5 A1.5 1.5 0 0 1 3.5 19 V12 A1.5 1.5 0 0 1 5 10.5 Z M13.5 14 H17 A1.5 1.5 0 0 1 18.5 15.5 V19 A1.5 1.5 0 0 1 17 20.5 H12",
        1.75f, 0.28f);
    i.activity = stroke_icon(ctx, "M3 12 H6.8 L9.5 5 L14.5 19 L17.2 12 H21", 1.8f);
    i.activity_active = duotone_icon(ctx,
        "M3 12 H6.8 L9.5 5 L14.5 19 L17.2 12 H21 Z",
        "M3 12 H6.8 L9.5 5 L14.5 19 L17.2 12 H21",
        1.9f, 0.14f);

    // Connection & Utilities
    i.plug = stroke_icon(ctx, "M12 22 V17 M9 8 V3 M15 8 V3 M6.5 8 H17.5 V11.5 A5.5 5.5 0 0 1 6.5 11.5 Z", 1.7f);
    i.plug_connected = duotone_icon(ctx,
        "M6.5 8 H17.5 V11.5 A5.5 5.5 0 0 1 6.5 11.5 Z",
        "M12 22 V17 M9 8 V3 M15 8 V3 M6.5 8 H17.5 V11.5 A5.5 5.5 0 0 1 6.5 11.5 Z",
        1.7f, 0.26f);
    i.plug_off = stroke_icon(ctx, "M3 3 L21 21 M9 8 V3 M15 8 V3 M12 22 V17 M17.5 8 V11.5 C17.5 12.6 17.2 13.6 16.6 14.4 M14.2 16.5 A5.5 5.5 0 0 1 6.5 11.5 V8 H8.5", 1.7f);
    i.info = stroke_icon(ctx, "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 M12 16 V11.5 M12 8.2 Z", 1.75f);
    i.close = stroke_icon(ctx, "M6.5 6.5 L17.5 17.5 M17.5 6.5 L6.5 17.5", 1.85f);
    i.copy = stroke_icon(ctx,
        "M10 8 H18 A2 2 0 0 1 20 10 V18 A2 2 0 0 1 18 20 H10 A2 2 0 0 1 8 18 V10 A2 2 0 0 1 10 8 Z M4 14.5 V6 A2 2 0 0 1 6 4 H14.5",
        1.7f);
    i.refresh = stroke_icon(ctx, "M20 12 A8 8 0 1 1 17.6 6.3 M20 4 V8.5 H15.5", 1.75f);
    i.trash = stroke_icon(ctx,
        "M4 6.5 H20 M9 6.5 V4.5 A1.5 1.5 0 0 1 10.5 3 H13.5 A1.5 1.5 0 0 1 15 4.5 V6.5 M6.5 6.5 L7.5 18.8 A2 2 0 0 0 9.5 20.6 H14.5 A2 2 0 0 0 16.5 18.8 L17.5 6.5 M10 11 V16 M14 11 V16",
        1.7f);
    i.external = stroke_icon(ctx,
        "M15 4 H20 V9 M20 4 L11 13 M18 14 V18.5 A2 2 0 0 1 16 20.5 H5.5 A2 2 0 0 1 3.5 18.5 V8 A2 2 0 0 1 5.5 6 H10",
        1.7f);
    i.arrow_up = stroke_icon(ctx, "M12 18.5 V5.5 M6.8 10.7 L12 5.5 L17.2 10.7", 1.9f);
    i.arrow_down = stroke_icon(ctx, "M12 5.5 V18.5 M6.8 13.3 L12 18.5 L17.2 13.3", 1.9f);
    i.bolt = stroke_icon(ctx, "M13 2.5 L4.5 13.5 H11.5 L10.5 21.5 L19.5 10.5 H12.5 Z", 1.7f);
    i.clock = stroke_icon(ctx, "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 M12 7.5 V12 L15.2 13.8", 1.7f);
    i.command = stroke_icon(ctx,
        "M15 6 V18 A3 3 0 1 0 18 15 H6 A3 3 0 1 0 9 18 V6 A3 3 0 1 0 6 9 H18 A3 3 0 1 0 15 6",
        1.7f);
    i.globe = stroke_icon(ctx,
        "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 M3.3 12 H20.7 M12 3 C14.6 6 14.6 18 12 21 M12 3 C9.4 6 9.4 18 12 21",
        1.65f);
    i.layers = stroke_icon(ctx,
        "M12 3.5 L20.5 8 L12 12.5 L3.5 8 Z M3.5 12.5 L12 17 L20.5 12.5 M3.5 16.5 L12 21 L20.5 16.5",
        1.7f);

    // Semantic Anti-Redundancy Icons (every feature & action has its own unmistakable glyph)
    i.square_pen = stroke_icon(ctx,
        "M12 4 H6 A2 2 0 0 0 4 6 V18 A2 2 0 0 0 6 20 H18 A2 2 0 0 0 20 18 V12 M18.4 3.6 A1.8 1.8 0 0 1 20.9 6.1 L12.5 14.5 L9 15.5 L10 12 Z",
        1.7f);
    i.plus_circle = stroke_icon(ctx,
        "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 M12 8 V16 M8 12 H16",
        1.7f);
    i.brain_circuit = stroke_icon(ctx,
        "M12 4.5 A3.2 3.2 0 0 0 6.8 6.8 A3.2 3.2 0 0 0 5.5 12.5 A3.2 3.2 0 0 0 8.5 18.2 H11.5 V4.5 M12.5 4.5 A3.2 3.2 0 0 1 17.7 6.8 M12.5 9.5 H16.5 M18.5 9.5 A1.5 1.5 0 1 0 18.5 9.51 M12.5 14.5 H15.5 L17.5 16.5 M18.8 17.8 A1.3 1.3 0 1 0 18.8 17.81",
        1.65f);
    i.brain_active = duotone_icon(ctx,
        "M11.5 4.5 A3.2 3.2 0 0 0 6.8 6.8 A3.2 3.2 0 0 0 5.5 12.5 A3.2 3.2 0 0 0 8.5 18.2 H11.5 Z",
        "M12 4.5 A3.2 3.2 0 0 0 6.8 6.8 A3.2 3.2 0 0 0 5.5 12.5 A3.2 3.2 0 0 0 8.5 18.2 H11.5 V4.5 M12.5 4.5 A3.2 3.2 0 0 1 17.7 6.8 M12.5 9.5 H16.5 M18.5 9.5 A1.5 1.5 0 1 0 18.5 9.51 M12.5 14.5 H15.5 L17.5 16.5",
        1.65f, 0.24f);
    i.gauge_effort = stroke_icon(ctx,
        "M4 17 A8 8 0 0 1 20 17 M12 16 L16.2 10.6 M12 16.5 A1.2 1.2 0 1 0 12 16.51 M6.8 12.2 L8 13 M12 9 V10.5",
        1.75f);
    i.wand_dust = stroke_icon(ctx,
        "M4 20 L15.5 8.5 M13 6 L18 11 M18.5 3.5 V6 M17.2 4.8 H19.8 M9 4 V6 M8 5 H10 M20 14 V16 M19 15 H21 M15 17.5 V19 M14.2 18.2 H15.8",
        1.7f);
    i.compress = stroke_icon(ctx,
        "M4 9 H9 V4 M20 9 H15 V4 M4 15 H9 V20 M20 15 H15 V20",
        1.75f);
    i.eye_open = stroke_icon(ctx,
        "M2.5 12 C5 7 8.5 5.5 12 5.5 C15.5 5.5 19 7 21.5 12 C19 17 15.5 18.5 12 18.5 C8.5 18.5 5 17 2.5 12 Z M12 9.2 A2.8 2.8 0 1 0 12 14.8 A2.8 2.8 0 1 0 12 9.2",
        1.7f);
    i.regex_search = stroke_icon(ctx,
        "M15 4 V11 M12 5.8 L18 9.2 M18 5.8 L12 9.2 M5.5 15 A3 3 0 1 0 11.5 15 A3 3 0 1 0 5.5 15 M11 17.5 L14 20.5 M17 17.5 H20",
        1.7f);
    i.file_plus = duotone_icon(ctx,
        "M6 3.5 H14 L18.5 8 V19 A1.5 1.5 0 0 1 17 20.5 H7 A1.5 1.5 0 0 1 5.5 19 V5 A1.5 1.5 0 0 1 7 3.5 Z",
        "M14 3.5 H7 A1.5 1.5 0 0 0 5.5 5 V19 A1.5 1.5 0 0 0 7 20.5 H17 A1.5 1.5 0 0 0 18.5 19 V8 Z M14 3.5 V8 H18.5 M12 11.5 V17 M9.2 14.2 H14.8",
        1.7f, 0.20f);
    i.file_code = stroke_icon(ctx,
        "M14 3.5 H7 A1.5 1.5 0 0 0 5.5 5 V19 A1.5 1.5 0 0 0 7 20.5 H17 A1.5 1.5 0 0 0 18.5 19 V8 Z M14 3.5 V8 H18.5 M10 12.5 L8.2 14.5 L10 16.5 M14 12.5 L15.8 14.5 L14 16.5",
        1.7f);
    i.image_frame = stroke_icon(ctx,
        "M5 4 H19 A2 2 0 0 1 21 6 V18 A2 2 0 0 1 19 20 H5 A2 2 0 0 1 3 18 V6 A2 2 0 0 1 5 4 Z M8.5 9.2 A1.3 1.3 0 1 0 8.5 9.21 M20.5 15.5 L15.5 10.5 L6.5 19.5",
        1.7f);
    i.git_compare = stroke_icon(ctx,
        "M6 4 A2 2 0 1 0 6 8 A2 2 0 1 0 6 4 M6 8 V16 A3 3 0 0 0 9 19 H12 M10 16.5 L12.5 19 L10 21.5 M18 20 A2 2 0 1 0 18 16 A2 2 0 1 0 18 20 M18 16 V8 A3 3 0 0 0 15 5 H12 M14 7.5 L11.5 5 L14 2.5",
        1.7f);
    i.git_fork = stroke_icon(ctx,
        "M6 5 A2 2 0 1 0 6 9 A2 2 0 1 0 6 5 M18 5 A2 2 0 1 0 18 9 A2 2 0 1 0 18 5 M12 15 A2 2 0 1 0 12 19 A2 2 0 1 0 12 15 M6 9 V10.5 C6 12.2 7.5 13 9 13 H15 C16.5 13 18 12.2 18 10.5 V9 M12 13 V15",
        1.75f);
    i.git_graph = stroke_icon(ctx,
        "M6 4 A2 2 0 1 0 6 8 A2 2 0 1 0 6 4 M6 16 A2 2 0 1 0 6 20 A2 2 0 1 0 6 16 M6 8 V16 M18 10 A2 2 0 1 0 18 14 A2 2 0 1 0 18 10 M6 12 H16 M12 12 C12 15 14 17 17.5 17.5",
        1.7f);
    i.git_graph_duotone = duotone_icon(ctx,
        "M6 4 A2 2 0 1 0 6 8 A2 2 0 1 0 6 4 M6 16 A2 2 0 1 0 6 20 A2 2 0 1 0 6 16 M18 10 A2 2 0 1 0 18 14 A2 2 0 1 0 18 10",
        "M6 4 A2 2 0 1 0 6 8 A2 2 0 1 0 6 4 M6 16 A2 2 0 1 0 6 20 A2 2 0 1 0 6 16 M6 8 V16 M18 10 A2 2 0 1 0 18 14 A2 2 0 1 0 18 10 M6 12 H16 M12 12 C12 15 14 17 17.5 17.5",
        1.7f, 0.34f);
    i.arrow_switch = stroke_icon(ctx,
        "M16 4 L20 8 L16 12 M20 8 H8 A4 4 0 0 0 4 12 V13 M8 20 L4 16 L8 12 M4 16 H16 A4 4 0 0 0 20 12 V11",
        1.7f);
    i.focus_fit = stroke_icon(ctx,
        "M4 9 V5.5 A1.5 1.5 0 0 1 5.5 4 H9 M15 4 H18.5 A1.5 1.5 0 0 1 20 5.5 V9 M20 15 V18.5 A1.5 1.5 0 0 1 18.5 20 H15 M9 20 H5.5 A1.5 1.5 0 0 1 4 18.5 V15 M12 9.5 A2.5 2.5 0 1 0 12 14.5 A2.5 2.5 0 1 0 12 9.5",
        1.7f);
    i.filter_lines = stroke_icon(ctx,
        "M4 6.5 H20 M7 12 H17 M10 17.5 H14",
        1.8f);
    i.zoom_in = stroke_icon(ctx,
        "M11 4 A7 7 0 1 0 11 18 A7 7 0 1 0 11 4 M20 20 L16 16 M11 8 V14 M8 11 H14",
        1.7f);
    i.zoom_out = stroke_icon(ctx,
        "M11 4 A7 7 0 1 0 11 18 A7 7 0 1 0 11 4 M20 20 L16 16 M8 11 H14",
        1.7f);
    i.type_reset = stroke_icon(ctx,
        "M5 6.5 V5 H19 V6.5 M12 5 V19 M9 19 H15",
        1.75f);
    i.sun_light = stroke_icon(ctx,
        "M12 8 A4 4 0 1 0 12 16 A4 4 0 1 0 12 8 M12 2.5 V4.5 M12 19.5 V21.5 M4.9 4.9 L6.4 6.4 M17.6 17.6 L19.1 19.1 M2.5 12 H4.5 M19.5 12 H21.5 M4.9 19.1 L6.4 17.6 M17.6 6.4 L19.1 4.9",
        1.7f);
    i.moon_dark = stroke_icon(ctx,
        "M19.5 14.2 A7.5 7.5 0 0 1 9.8 4.5 A8 8 0 1 0 19.5 14.2 Z",
        1.7f);
    i.gem_mineral = stroke_icon(ctx,
        "M6 4.5 H18 L21 9.5 L12 20 L3 9.5 Z M3 9.5 H21 M12 20 L8.5 9.5 L12 4.5 L15.5 9.5 Z",
        1.65f);
    i.feather_linen = stroke_icon(ctx,
        "M19.5 4.5 C15 4.5 10.5 7.2 8 11.5 L4.5 19.5 L12.5 16 C16.8 13.5 19.5 9 19.5 4.5 Z M4.5 19.5 L14.5 9.5",
        1.7f);
    i.languages = stroke_icon(ctx,
        "M4 6 H13 M8.5 4 V6 M11 6 C10.4 9.5 8 12.8 4.5 14.5 M6.5 9.5 C7.8 11.4 9.6 13 11.8 14 M13 19.5 L16.5 10.5 L20 19.5 M14.2 16.5 H18.8",
        1.65f);
    i.lock_shield = stroke_icon(ctx,
        "M12 3 L19 6 V11.5 C19 16 16 19.4 12 21 C8 19.4 5 16 5 11.5 V6 Z M9.5 11.5 H14.5 V15 H9.5 Z M10.5 11.5 V10 A1.5 1.5 0 0 1 13.5 10 V11.5",
        1.65f);
    i.shield_check = duotone_icon(ctx,
        "M12 3 L19 6 V11.5 C19 16 16 19.4 12 21 C8 19.4 5 16 5 11.5 V6 Z",
        "M12 3 L19 6 V11.5 C19 16 16 19.4 12 21 C8 19.4 5 16 5 11.5 V6 Z M9 12.2 L11.2 14.4 L15.5 9.8",
        1.7f, 0.22f);
    i.file_pen = stroke_icon(ctx,
        "M13 3.5 H7 A1.5 1.5 0 0 0 5.5 5 V19 A1.5 1.5 0 0 0 7 20.5 H16.5 A1.5 1.5 0 0 0 18 19 V14 M19 5.5 A1.5 1.5 0 0 0 16.9 3.4 L11 9.3 L10.2 12.3 L13.2 11.5 Z",
        1.68f);
    i.zap_warning = stroke_icon(ctx,
        "M12 3.5 L21 19 H3 Z M12 9.5 V13.5 M12 16.2 Z",
        1.75f);
    i.terminal_check = stroke_icon(ctx,
        "M4 5.5 H20 A1.5 1.5 0 0 1 21.5 7 V17 A1.5 1.5 0 0 1 20 18.5 H4 A1.5 1.5 0 0 1 2.5 17 V7 A1.5 1.5 0 0 1 4 5.5 Z M6.5 10 L9 12.2 L6.5 14.4 M12.2 13.5 L14.2 15.2 L17.8 10.6",
        1.7f);
    i.badge_check = duotone_icon(ctx,
        "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 Z",
        "M8.2 12.2 L10.8 14.8 L15.8 9.2",
        1.9f, 0.22f);
    i.user_circle = stroke_icon(ctx,
        "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 M12 8 A2.6 2.6 0 1 0 12 13.2 A2.6 2.6 0 1 0 12 8 M7.2 18.2 C8.3 16 10 15 12 15 C14 15 15.7 16 16.8 18.2",
        1.65f);
    i.bot_node = stroke_icon(ctx,
        "M12 4 V7 M9.5 4 H14.5 M5.5 7 H18.5 A2 2 0 0 1 20.5 9 V17 A2 2 0 0 1 18.5 19 H5.5 A2 2 0 0 1 3.5 17 V9 A2 2 0 0 1 5.5 7 Z M9.2 12.5 Z M14.8 12.5 Z M9.5 15.8 H14.5",
        1.7f);
    i.wrench_node = stroke_icon(ctx,
        "M14.7 6.3 A4 4 0 0 0 9.4 11.6 L4.5 16.5 A1.8 1.8 0 0 0 7 19 L11.9 14.1 A4 4 0 0 0 17.2 8.8 L15 11 L13 9 Z",
        1.7f);

    // Native Window Caption Controls (precision-tuned strokes)
    i.win_min     = stroke_icon(ctx, "M6 12 H18", 1.55f);
    i.win_max     = stroke_icon(ctx, "M7 6 H17 A1 1 0 0 1 18 7 V17 A1 1 0 0 1 17 18 H7 A1 1 0 0 1 6 17 V7 A1 1 0 0 1 7 6 Z", 1.55f);
    i.win_restore = stroke_icon(ctx, "M8 8.5 H15.5 A1 1 0 0 1 16.5 9.5 V17 A1 1 0 0 1 15.5 18 H8 A1 1 0 0 1 7 17 V9.5 A1 1 0 0 1 8 8.5 Z M10 8.5 V6.8 A1 1 0 0 1 11 5.8 H17.5 A1 1 0 0 1 18.5 6.8 V13.5 A1 1 0 0 1 17.5 14.5 H16.5", 1.5f);
    i.win_close   = stroke_icon(ctx, "M7 7 L17 17 M17 7 L7 17", 1.65f);
    return i;
}

} // namespace ompe
