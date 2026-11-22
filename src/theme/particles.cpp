#include "theme/particles.h"

#include <algorithm>
#include <cmath>

namespace ompe {

float ParticleSystem::rand01() noexcept {
    uint32_t x = rng_state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state_ = x ? x : 0x9E3779B9u;
    return static_cast<float>(x & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

uint32_t ParticleSystem::scaled_count(uint32_t base) const noexcept {
    if (density_ == ParticleDensity::Off) return 0;
    if (density_ == ParticleDensity::Subtle) return std::max(6u, base / 2u);
    return base;
}

void ParticleSystem::spawn_dissolve_rect(etch::Rect rect,
                                         etch::Color base_color,
                                         etch::Color accent_color,
                                         DissolveStyle style,
                                         uint32_t base_count) {
    const uint32_t n = scaled_count(base_count);
    if (n == 0 || rect.w <= 1.0f || rect.h <= 1.0f) return;

    // Keep total active particles bounded for butter-smooth 120fps even during rapid clicks
    if (particles_.size() > 900) {
        particles_.erase(particles_.begin(), particles_.begin() + 250);
    }

    const etch::Vec2 center{rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f};
    particles_.reserve(particles_.size() + n);

    for (uint32_t i = 0; i < n; ++i) {
        const float u = rand01();
        const float v = rand01();
        Particle p{};
        p.pos = {rect.x + u * rect.w, rect.y + v * rect.h};
        p.anchor = center;
        p.style = style;
        p.curl_seed = rand01() * 6.283185f;

        // Progressive dissolve wave across left-to-right (staggered negative age = organic sweep!)
        const float sweep_delay = (u * 0.14f + rand01() * 0.06f);
        p.age = -sweep_delay;

        const bool is_hot = (rand01() < 0.34f);
        p.color = is_hot ? accent_color : etch::Color::lerp_oklab(base_color, accent_color, 0.25f + rand01() * 0.35f);
        p.hot_color = accent_color;

        switch (style) {
            case DissolveStyle::Dust:
                p.vel = {18.0f + rand01() * 58.0f, -24.0f - rand01() * 64.0f};
                p.size_px = 1.6f + rand01() * 2.4f;
                p.life = 0.42f + rand01() * 0.36f;
                p.drag = 2.4f;
                p.gravity = -12.0f;
                p.curl_amp = 28.0f;
                break;
            case DissolveStyle::Ember:
                p.vel = {(rand01() - 0.35f) * 60.0f, -48.0f - rand01() * 88.0f};
                p.size_px = 1.8f + rand01() * 2.6f;
                p.life = 0.45f + rand01() * 0.40f;
                p.drag = 1.9f;
                p.gravity = -22.0f;
                p.curl_amp = 42.0f;
                break;
            case DissolveStyle::Sand:
                p.vel = {(rand01() - 0.45f) * 44.0f, -8.0f + rand01() * 26.0f};
                p.size_px = 1.5f + rand01() * 2.0f;
                p.life = 0.40f + rand01() * 0.32f;
                p.drag = 1.6f;
                p.gravity = 135.0f;
                p.curl_amp = 14.0f;
                break;
            case DissolveStyle::Vortex: {
                const float dx = p.pos.x - center.x;
                const float dy = p.pos.y - center.y;
                p.vel = {-dy * 0.85f - dx * 0.55f, dx * 0.85f - dy * 0.55f - 36.0f};
                p.size_px = 1.7f + rand01() * 2.2f;
                p.life = 0.48f + rand01() * 0.30f;
                p.drag = 2.8f;
                p.gravity = -16.0f;
                p.curl_amp = 32.0f;
                break;
            }
        }
        particles_.push_back(p);
    }
}

void ParticleSystem::spawn_send_burst(etch::Rect composer_top_edge,
                                      etch::Color accent_color,
                                      etch::Color secondary_color) {
    const uint32_t n = scaled_count(44);
    if (n == 0 || composer_top_edge.w <= 4.0f) return;

    particles_.reserve(particles_.size() + n);
    for (uint32_t i = 0; i < n; ++i) {
        const float u = rand01();
        // Bell-curve density toward center of composer top rim
        const float spread = 0.5f + (u - 0.5f) * (0.65f + 0.35f * rand01());
        Particle p{};
        p.pos = {composer_top_edge.x + spread * composer_top_edge.w,
                 composer_top_edge.y + (rand01() - 0.5f) * 4.0f};
        p.vel = {(spread - 0.5f) * 42.0f + (rand01() - 0.5f) * 18.0f,
                 -52.0f - rand01() * 92.0f};
        p.color = (rand01() < 0.7f) ? accent_color : secondary_color;
        p.hot_color = accent_color;
        p.size_px = 1.6f + rand01() * 2.3f;
        p.age = -rand01() * 0.08f;
        p.life = 0.40f + rand01() * 0.32f;
        p.drag = 2.9f;
        p.gravity = -18.0f;
        p.curl_seed = rand01() * 6.283185f;
        p.curl_amp = 24.0f;
        p.style = DissolveStyle::Ember;
        particles_.push_back(p);
    }
}

void ParticleSystem::spawn_settle_sparkle(etch::Vec2 origin,
                                          float span_width,
                                          etch::Color accent_color,
                                          etch::Color secondary_color) {
    const uint32_t n = scaled_count(22);
    if (n == 0) return;
    particles_.reserve(particles_.size() + n);
    for (uint32_t i = 0; i < n; ++i) {
        const float u = rand01();
        Particle p{};
        p.pos = {origin.x + u * span_width, origin.y + (rand01() - 0.5f) * 6.0f};
        p.vel = {(rand01() - 0.5f) * 16.0f, -14.0f - rand01() * 28.0f};
        p.color = (rand01() < 0.55f) ? secondary_color : accent_color;
        p.hot_color = accent_color;
        p.size_px = 1.5f + rand01() * 1.9f;
        p.age = -u * 0.16f;
        p.life = 0.48f + rand01() * 0.28f;
        p.drag = 3.2f;
        p.gravity = -6.0f;
        p.curl_seed = rand01() * 6.283185f;
        p.curl_amp = 14.0f;
        p.style = DissolveStyle::Dust;
        particles_.push_back(p);
    }
}

void ParticleSystem::spawn_point_spark(etch::Vec2 center, etch::Color accent_color, uint32_t count) {
    const uint32_t n = scaled_count(count);
    if (n == 0) return;
    particles_.reserve(particles_.size() + n);
    for (uint32_t i = 0; i < n; ++i) {
        const float ang = (static_cast<float>(i) / static_cast<float>(n)) * 6.283185f + (rand01() - 0.5f) * 0.4f;
        const float spd = 32.0f + rand01() * 68.0f;
        Particle p{};
        p.pos = center;
        p.vel = {std::cos(ang) * spd, std::sin(ang) * spd - 12.0f};
        p.color = accent_color;
        p.hot_color = etch::Color::white();
        p.size_px = 1.6f + rand01() * 1.8f;
        p.age = 0.0f;
        p.life = 0.30f + rand01() * 0.22f;
        p.drag = 4.6f;
        p.gravity = 24.0f;
        p.curl_seed = rand01() * 6.283185f;
        p.curl_amp = 10.0f;
        p.style = DissolveStyle::Ember;
        particles_.push_back(p);
    }
}

bool ParticleSystem::update(float dt_seconds) noexcept {
    if (particles_.empty()) return false;
    const float dt = std::clamp(dt_seconds, 0.001f, 0.04f);

    size_t write_idx = 0;
    for (size_t i = 0; i < particles_.size(); ++i) {
        Particle& p = particles_[i];
        p.age += dt;
        if (p.age >= p.life) continue;
        if (p.age >= 0.0f) {
            const float damp = std::exp(-p.drag * dt);
            p.vel.x *= damp;
            p.vel.y = p.vel.y * damp + p.gravity * dt;

            // Smooth organic curl-like lateral drift
            const float phase = p.curl_seed + p.age * 7.5f;
            const float curl_x = std::cos(phase) * p.curl_amp;
            const float curl_y = std::sin(phase * 0.85f) * (p.curl_amp * 0.5f);

            p.pos.x += (p.vel.x + curl_x) * dt;
            p.pos.y += (p.vel.y + curl_y) * dt;
        }
        if (write_idx != i) particles_[write_idx] = p;
        ++write_idx;
    }
    particles_.resize(write_idx);
    return !particles_.empty();
}

void ParticleSystem::render(bool dark_mode) const noexcept {
    if (particles_.empty()) return;

    // On dark themes, additive-light ember blend gives realistic optical glow without heavy bloom pass
    etch::LayerDesc layer{};
    layer.opacity = 1.0f;
    layer.blend_plus_lighter = dark_mode;
    etch::push_layer(layer);

    for (const auto& p : particles_) {
        if (p.age < 0.0f) continue;
        const float t = std::clamp(p.age / p.life, 0.0f, 1.0f);
        // Quick ignite then smooth cubic fade
        const float fade_in = std::min(1.0f, t * 8.0f);
        const float fade_out = (1.0f - t) * (1.0f - t);
        const float alpha = fade_in * fade_out;
        if (alpha <= 0.01f) continue;

        // Ember/hot edge flash early in particle life, then cools into mineral dust color
        etch::Color c = (p.style == DissolveStyle::Ember && t < 0.35f)
            ? etch::Color::lerp_oklab(p.hot_color, p.color, t / 0.35f)
            : p.color;
        c.a = static_cast<uint8_t>(std::clamp(alpha * (dark_mode ? 235.0f : 205.0f), 0.0f, 255.0f));

        const float sz = p.size_px * (1.0f - 0.55f * t);
        if (sz < 0.6f) continue;

        etch::draw_rect(etch::RectDrawDesc{
            .rect = {p.pos.x - sz * 0.5f, p.pos.y - sz * 0.5f, sz, sz},
            .fill = c,
            .radius = etch::Corners::all(sz * 0.5f),
            .foreground = true,
            .softness = 1.25f,
        });
    }

    etch::pop_layer();
}

} // namespace ompe
