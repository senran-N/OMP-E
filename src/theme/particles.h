#pragma once

#include <etch/etch.h>
#include <cstdint>
#include <vector>

namespace ompe {

enum class DissolveStyle : uint8_t {
    Dust = 0,   // Fine upward-drifting mineral motes (like dissolve.rselmi.com dust)
    Ember,      // Glowing warm sparks rising with curl turbulence
    Sand,       // Tactile grains cascading gently with soft gravity
    Vortex,     // Inward/upward spiral pull into the conversation stream
};

enum class ParticleDensity : uint8_t {
    Off = 0,
    Subtle = 1,
    Signature = 2,
};

struct Particle {
    etch::Vec2 pos{};
    etch::Vec2 vel{};
    etch::Vec2 anchor{};
    etch::Color color{};
    etch::Color hot_color{};
    float size_px = 2.0f;
    float age = 0.0f;
    float life = 0.55f;
    float drag = 2.6f;
    float gravity = 0.0f;
    float curl_seed = 0.0f;
    float curl_amp = 18.0f;
    DissolveStyle style = DissolveStyle::Dust;
};

// Zero-clutter, GPU-batched micro-interaction particle system (renders in 1 Etch draw call).
// Automatically sleeps (needs_redraw = false) the instant all particles fade out.
class ParticleSystem {
public:
    void set_density(ParticleDensity d) noexcept { density_ = d; }
    ParticleDensity density() const noexcept { return density_; }

    // Dissolve an exact UI element rectangle into dust/ember/sand/vortex particles on dismiss/delete/abort
    void spawn_dissolve_rect(etch::Rect rect,
                             etch::Color base_color,
                             etch::Color accent_color,
                             DissolveStyle style = DissolveStyle::Dust,
                             uint32_t base_count = 72);

    // Directional upward lift burst when sending a prompt from the composer
    void spawn_send_burst(etch::Rect composer_top_edge,
                          etch::Color accent_color,
                          etch::Color secondary_color);

    // Subtle pinpoint shimmer when a streaming turn settles cleanly
    void spawn_settle_sparkle(etch::Vec2 origin,
                              float span_width,
                              etch::Color accent_color,
                              etch::Color secondary_color);

    // Tactile micro-burst on button confirmation / copy checkmark / provider added
    void spawn_point_spark(etch::Vec2 center, etch::Color accent_color, uint32_t count = 18);

    // Advance physics by dt; returns true while particles are alive (keep animating)
    bool update(float dt_seconds) noexcept;

    // Emit GPU primitives inside a root overlay box (uses Etch batching, zero heap alloc)
    void render(bool dark_mode) const noexcept;

    bool active() const noexcept { return !particles_.empty(); }
    size_t count() const noexcept { return particles_.size(); }
    void clear() noexcept { particles_.clear(); }

private:
    float rand01() noexcept;
    uint32_t scaled_count(uint32_t base) const noexcept;

    std::vector<Particle> particles_;
    ParticleDensity density_ = ParticleDensity::Signature;
    uint32_t rng_state_ = 0xA341316Cu;
};

} // namespace ompe
