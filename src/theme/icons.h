#pragma once

#include <etch/etch.h>
#include <cstdint>

namespace ompe {

struct AppIcons {
    // Brand & Panel state-reactive toggles
    uint16_t logo              = etch::kInvalidIcon;
    uint16_t panel_left_open   = etch::kInvalidIcon;
    uint16_t panel_left_close  = etch::kInvalidIcon;
    uint16_t panel_right_open  = etch::kInvalidIcon;
    uint16_t panel_right_close = etch::kInvalidIcon;

    // Navigation & Workspace (outline + active duotone)
    uint16_t plus              = etch::kInvalidIcon;
    uint16_t search            = etch::kInvalidIcon;
    uint16_t folder            = etch::kInvalidIcon;
    uint16_t folder_open       = etch::kInvalidIcon;
    uint16_t pin               = etch::kInvalidIcon;
    uint16_t pin_filled        = etch::kInvalidIcon;
    uint16_t message           = etch::kInvalidIcon;

    // Disclosures & Animated Status
    uint16_t chevron_r         = etch::kInvalidIcon;
    uint16_t chevron_d         = etch::kInvalidIcon;
    uint16_t loader_arc        = etch::kInvalidIcon;
    uint16_t check             = etch::kInvalidIcon;
    uint16_t check_circle      = etch::kInvalidIcon;
    uint16_t x_circle          = etch::kInvalidIcon;
    uint16_t alert             = etch::kInvalidIcon;

    // Composer & Voice state-reactive controls
    uint16_t send              = etch::kInvalidIcon;
    uint16_t stop              = etch::kInvalidIcon;
    uint16_t steer             = etch::kInvalidIcon;
    uint16_t sparkle           = etch::kInvalidIcon;
    uint16_t sparkle_active    = etch::kInvalidIcon;
    uint16_t paperclip         = etch::kInvalidIcon;
    uint16_t mic               = etch::kInvalidIcon;
    uint16_t audio_lines       = etch::kInvalidIcon;

    // Semantic Tool & Workbench Icons (outline + duotone)
    uint16_t terminal          = etch::kInvalidIcon;
    uint16_t terminal_duotone  = etch::kInvalidIcon;
    uint16_t code              = etch::kInvalidIcon;
    uint16_t file              = etch::kInvalidIcon;
    uint16_t file_text         = etch::kInvalidIcon;
    uint16_t file_diff         = etch::kInvalidIcon;
    uint16_t file_diff_duotone = etch::kInvalidIcon;
    uint16_t edit              = etch::kInvalidIcon;
    uint16_t folder_search     = etch::kInvalidIcon;
    uint16_t list_checks       = etch::kInvalidIcon;
    uint16_t branch            = etch::kInvalidIcon;
    uint16_t branch_duotone    = etch::kInvalidIcon;
    uint16_t agents            = etch::kInvalidIcon;
    uint16_t agents_duotone    = etch::kInvalidIcon;
    uint16_t btw               = etch::kInvalidIcon;
    uint16_t btw_duotone       = etch::kInvalidIcon;
    uint16_t maximize          = etch::kInvalidIcon;

    // Settings Navigation (outline + active duotone)
    uint16_t settings          = etch::kInvalidIcon;
    uint16_t sliders           = etch::kInvalidIcon;
    uint16_t sliders_active    = etch::kInvalidIcon;
    uint16_t key               = etch::kInvalidIcon;
    uint16_t key_active        = etch::kInvalidIcon;
    uint16_t cpu               = etch::kInvalidIcon;
    uint16_t cpu_active        = etch::kInvalidIcon;
    uint16_t palette           = etch::kInvalidIcon;
    uint16_t palette_active    = etch::kInvalidIcon;
    uint16_t shield            = etch::kInvalidIcon;
    uint16_t shield_active     = etch::kInvalidIcon;
    uint16_t blocks            = etch::kInvalidIcon;
    uint16_t blocks_active     = etch::kInvalidIcon;
    uint16_t activity          = etch::kInvalidIcon;
    uint16_t activity_active   = etch::kInvalidIcon;

    // Connection & Utility
    uint16_t plug              = etch::kInvalidIcon;
    uint16_t plug_connected    = etch::kInvalidIcon;
    uint16_t plug_off          = etch::kInvalidIcon;
    uint16_t info              = etch::kInvalidIcon;
    uint16_t close             = etch::kInvalidIcon;
    uint16_t copy              = etch::kInvalidIcon;
    uint16_t refresh           = etch::kInvalidIcon;
    uint16_t trash             = etch::kInvalidIcon;
    uint16_t external          = etch::kInvalidIcon;
    uint16_t arrow_up          = etch::kInvalidIcon;
    uint16_t arrow_down        = etch::kInvalidIcon;
    uint16_t bolt              = etch::kInvalidIcon;
    uint16_t clock             = etch::kInvalidIcon;
    uint16_t command           = etch::kInvalidIcon;
    uint16_t globe             = etch::kInvalidIcon;
    uint16_t layers            = etch::kInvalidIcon;

    // Semantic Anti-Redundancy Icons (Bespoke per Feature / Action / Role)
    uint16_t square_pen        = etch::kInvalidIcon; // New thread / compose
    uint16_t plus_circle       = etch::kInvalidIcon; // Add provider / add MCP server
    uint16_t brain_circuit     = etch::kInvalidIcon; // Reasoning capability / thinking
    uint16_t brain_active      = etch::kInvalidIcon; // Active reasoning (duotone)
    uint16_t gauge_effort      = etch::kInvalidIcon; // Thinking effort switcher
    uint16_t wand_dust         = etch::kInvalidIcon; // Dissolve-to-dust GPU particle craft
    uint16_t compress          = etch::kInvalidIcon; // Context compaction / summary notice
    uint16_t eye_open          = etch::kInvalidIcon; // Read / inspect file trace
    uint16_t regex_search      = etch::kInvalidIcon; // Code / grep pattern search
    uint16_t file_plus         = etch::kInvalidIcon; // Write / create file mutation
    uint16_t file_code         = etch::kInvalidIcon; // Code / file attachment chip
    uint16_t image_frame       = etch::kInvalidIcon; // Image attachment chip
    uint16_t git_compare       = etch::kInvalidIcon; // Review diff starter
    uint16_t git_fork          = etch::kInvalidIcon; // Fork thread / branch
    uint16_t git_graph         = etch::kInvalidIcon; // Session tree DAG (idle)
    uint16_t git_graph_duotone = etch::kInvalidIcon; // Session tree DAG (active)
    uint16_t arrow_switch      = etch::kInvalidIcon; // Switch branch target
    uint16_t focus_fit         = etch::kInvalidIcon; // Fit / reset canvas viewport
    uint16_t filter_lines      = etch::kInvalidIcon; // Filter tree entries (Turns / All)
    uint16_t zoom_in           = etch::kInvalidIcon; // Increase text scale
    uint16_t zoom_out          = etch::kInvalidIcon; // Decrease text scale
    uint16_t type_reset        = etch::kInvalidIcon; // Reset typography scale
    uint16_t sun_light         = etch::kInvalidIcon; // Light mineral palette
    uint16_t moon_dark         = etch::kInvalidIcon; // Dark obsidian palette
    uint16_t gem_mineral       = etch::kInvalidIcon; // Celadon mineral palette
    uint16_t feather_linen     = etch::kInvalidIcon; // Nordic linen palette
    uint16_t languages         = etch::kInvalidIcon; // i18n language selector
    uint16_t lock_shield       = etch::kInvalidIcon; // Approval: Always Ask (strict)
    uint16_t shield_check      = etch::kInvalidIcon; // Approval: Ask & Remember / OAuth verified
    uint16_t file_pen          = etch::kInvalidIcon; // Approval: Auto-Write, Ask Shell
    uint16_t zap_warning       = etch::kInvalidIcon; // Approval: Autonomous (YOLO)
    uint16_t terminal_check    = etch::kInvalidIcon; // Succeeded shell command
    uint16_t badge_check       = etch::kInvalidIcon; // Active option check badge
    uint16_t user_circle       = etch::kInvalidIcon; // Tree node: User prompt
    uint16_t bot_node          = etch::kInvalidIcon; // Tree node: Assistant turn
    uint16_t wrench_node       = etch::kInvalidIcon; // Tree node: Tool execution

    // Native Window Caption Controls
    uint16_t win_min           = etch::kInvalidIcon;
    uint16_t win_max           = etch::kInvalidIcon;
    uint16_t win_restore       = etch::kInvalidIcon;
    uint16_t win_close         = etch::kInvalidIcon;
};

AppIcons register_app_icons(etch::Context* ctx) noexcept;

} // namespace ompe
