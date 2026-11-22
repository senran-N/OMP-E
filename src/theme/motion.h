#pragma once

#include <etch/etch.h>

namespace ompe {

// Semantic motion curves: responsive, physically grounded, never distracting.
inline constexpr etch::SpringSpec kMotionSnap    = { .response = 0.14f, .damping_ratio = 0.98f };
inline constexpr etch::SpringSpec kMotionFluid   = { .response = 0.24f, .damping_ratio = 0.94f };
inline constexpr etch::SpringSpec kMotionGentle  = { .response = 0.36f, .damping_ratio = 1.00f };
inline constexpr etch::SpringSpec kMotionTactile = { .response = 0.22f, .damping_ratio = 0.78f };

// Consistent spatial rhythm (4px base grid)
inline constexpr float kRadiusXs   = 5.0f;
inline constexpr float kRadiusSm   = 8.0f;
inline constexpr float kRadiusMd   = 12.0f;
inline constexpr float kRadiusLg   = 16.0f;
inline constexpr float kRadiusPill = 999.0f;

inline constexpr float kTitlebarH  = 44.0f;
inline constexpr float kSidebarW   = 276.0f;
inline constexpr float kInspectorW = 368.0f;
inline constexpr float kReadingMaxW = 840.0f;

} // namespace ompe
