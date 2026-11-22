// OMP-E Bespoke Architectural Silk, Fine Crystalline Particle Dust & Acoustic Ribbon Shader
// Gallery-grade minimalism (linear R16G16B16A16F output):
//   - True C2-continuous Perlin atmospheric whisper (calibrated for linear HDR space)
//   - Pinpoint crystalline mineral dust grains with cursor gravity
//   - Seamless perimeter fade to exact base_col (zero seam above composer, zero screen-door grain)
// Uniforms (80 bytes at b0/space3):
//   viewport_time = (width, height, time_sec, frame_index)
//   params[0]     = base_rgb (linear), dark_mode_flag (1.0 = dark, 0.0 = light)
//   params[1]     = accent_rgb (linear), energy (0.0 = calm ambient, 1.0 = streaming/active)
//   params[2]     = secondary_rgb (linear), voice_level (0..1 RMS)
//   params[3]     = pointer_uv (x, y), particle_strength (z: 0..1), mode (w: 0=ambient+particles, 1=voice ribbon)

#ifdef __spirv__
[[vk::binding(0, 3)]]
#endif
cbuffer SurfaceFS : register(b0, space3)
{
    float4 viewport_time;
    float4 params[4];
};

float hash21(float2 p)
{
    p = frac(p * float2(127.1f, 311.7f));
    p += dot(p, p + 34.23f);
    return frac(p.x * p.y);
}

float2 hash22(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031f, 0.1030f, 0.0973f));
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.xx + p3.yz) * p3.zy);
}

// True C2-continuous 2D gradient Perlin noise (zero grid creases or angular shards)
float2 grad2(float2 cell)
{
    float a = hash21(cell) * 6.2831853f;
    return float2(cos(a), sin(a));
}

float perlin2d(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    float n00 = dot(grad2(i + float2(0.0f, 0.0f)), f - float2(0.0f, 0.0f));
    float n10 = dot(grad2(i + float2(1.0f, 0.0f)), f - float2(1.0f, 0.0f));
    float n01 = dot(grad2(i + float2(0.0f, 1.0f)), f - float2(0.0f, 1.0f));
    float n11 = dot(grad2(i + float2(1.0f, 1.0f)), f - float2(1.0f, 1.0f));

    return lerp(lerp(n00, n10, u.x), lerp(n01, n11, u.x), u.y) * 0.5f + 0.5f;
}

float smooth_fbm(float2 p)
{
    float v = 0.0f;
    float amp = 0.5f;
    float2x2 rot = float2x2(0.80f, -0.60f, 0.60f, 0.80f);
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        v += amp * perlin2d(p);
        p = mul(rot, p) * 2.01f + float2(7.3f, 11.9f);
        amp *= 0.5f;
    }
    return v;
}

// Pinpoint crystalline mineral dust motes (calibrated for linear float render target)
float dust_plane(float2 uv_aspect, float scale, float t, float2 ptr_aspect, float seed, float grain_size)
{
    float2 drift = float2(t * 0.018f, -t * 0.032f);
    float2 g = (uv_aspect + drift) * scale;
    float2 id = floor(g);
    float2 gv = frac(g) - 0.5f;

    float acc = 0.0f;
    [unroll]
    for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx)
        {
            float2 offs = float2(dx, dy);
            float2 cell = id + offs;
            float2 rnd = hash22(cell + seed);
            // Curated sparsity (~22% occupancy)
            if (rnd.x < 0.78f) continue;

            float phase = rnd.y * 6.283185f + t * (0.40f + rnd.x * 0.55f);
            float2 jitter = (rnd - 0.5f) * 0.78f + float2(cos(phase), sin(phase * 0.82f)) * 0.10f;
            float2 pos = offs + jitter;

            // Gentle magnetic pull near cursor
            float2 world_pt = (cell + 0.5f + pos) / scale - drift;
            float2 to_ptr = ptr_aspect - world_pt;
            float pull = exp(-dot(to_ptr, to_ptr) * 24.0f);
            pos += to_ptr * pull * 0.24f;

            float d = length(gv - pos);
            float r = grain_size * lerp(0.65f, 1.45f, frac(rnd.x * 31.7f));
            // Razor-crisp jewel pinpoint + ultra-tight micro halo (zero blurry bokeh blobs)
            float core = smoothstep(r, r * 0.14f, d);
            float halo = exp(-d * d / max(r * r * 3.4f, 0.00008f)) * (0.10f + pull * 0.28f);
            float twinkle = 0.35f + 0.65f * pow(0.5f + 0.5f * sin(phase * 1.25f), 2.0f);
            // Natural brightness hierarchy: delicate background dust + gleaming champagne micro-sparks
            float gleam = lerp(0.35f, 1.0f, pow(frac(rnd.y * 19.3f), 1.4f));
            acc += (core * 1.55f + halo) * twinkle * gleam;
        }
    }
    return acc;
}

float4 ps_main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target0
{
    float t = viewport_time.z;
    float3 base_col = params[0].xyz;
    float is_dark = params[0].w;
    float3 accent = params[1].xyz;
    float energy = saturate(params[1].w);
    float3 secondary = params[2].xyz;
    float voice = saturate(params[2].w);
    float2 ptr = params[3].xy;
    float particle_amt = saturate(params[3].z);
    float mode = params[3].w;

    float aspect = max(viewport_time.x, 1.0f) / max(viewport_time.y, 1.0f);
    float2 p = (uv - 0.5f) * float2(aspect, 1.0f);
    float2 ptr_p = (ptr - 0.5f) * float2(aspect, 1.0f);

    float3 col = base_col;

    if (mode > 0.5f)
    {
        // Acoustic Silk Ribbon + Resonant Micro-Grains (Live Voice)
        float amp = 0.04f + voice * 0.24f + energy * 0.06f;
        float w1 = sin(uv.x * 7.2f - t * 2.8f) * cos(uv.x * 3.1f + t * 1.2f) * amp;
        float w2 = sin(uv.x * 12.0f + t * 3.6f) * (amp * 0.52f);
        float d1 = abs(uv.y - 0.5f - w1);
        float d2 = abs(uv.y - 0.5f - w2);
        float ribbon1 = exp(-d1 * 34.0f) * 0.55f;
        float ribbon2 = exp(-d2 * 48.0f) * 0.35f;
        float halo = exp(-d1 * 7.5f) * (0.04f + voice * 0.10f);
        float edge_fade = smoothstep(0.0f, 0.14f, uv.x) * smoothstep(1.0f, 0.86f, uv.x);
        float dust = dust_plane(p, 16.0f, t * 1.4f, ptr_p, 11.0f, 0.018f) * exp(-d1 * 6.5f) * particle_amt * 0.40f;
        col += (accent * (ribbon1 + halo + dust) + secondary * ribbon2) * edge_fade;
    }
    else
    {
        // Generous perimeter falloff -> strictly 0.0 at all 4 borders (invisible merge into canvas)
        float edge_env = smoothstep(0.02f, 0.22f, uv.y) * smoothstep(0.96f, 0.68f, uv.y) *
                         smoothstep(0.02f, 0.16f, uv.x) * smoothstep(0.98f, 0.84f, uv.x);

        // Aspect-true radial glow centered delicately behind the hero emblem
        float2 c_dist = (uv - float2(0.50f, 0.45f)) * float2(aspect, 1.0f);
        float r2 = dot(c_dist, c_dist);
        float center_aura = exp(-r2 * 5.2f);
        float wide_aura   = exp(-r2 * 1.75f);

        // Whisper-quiet C2 Perlin silk drift (linear HDR calibrated: 0.014..0.028)
        float slow_t = t * (0.10f + energy * 0.20f);
        float f1 = smooth_fbm(p * 1.15f + float2(slow_t * 0.30f, -slow_t * 0.18f));
        float f2 = smooth_fbm(p * 1.45f + float2(-slow_t * 0.22f, slow_t * 0.26f) + float2(3.7f, 2.1f));
        float silk = smoothstep(0.30f, 0.80f, f1 * 0.58f + f2 * 0.42f);

        // Delicate pointer warmth
        float2 dp = p - ptr_p;
        float ptr_glow = exp(-dot(dp, dp) * 9.0f) * 0.022f;

        // 3 depth planes of ultra-fine crystalline mineral dust (no blurred ring, pure starfield craft)
        float d_far  = dust_plane(p, 28.0f, t * 0.40f, ptr_p,  3.1f, 0.016f) * 0.28f;
        float d_mid  = dust_plane(p, 17.5f, t * 0.70f, ptr_p, 19.7f, 0.018f) * 0.48f;
        float d_near = dust_plane(p, 10.5f, t * 1.00f, ptr_p, 47.3f, 0.015f) * 0.65f;

        // Natural radial constellation density (gently concentrated near center, sparse toward margins)
        float dust_mask = (0.14f + 0.86f * wide_aura) * edge_env * particle_amt;
        float3 tint = lerp(secondary, accent, saturate(0.35f + f2 * 0.55f));

        if (is_dark > 0.5f)
        {
            // Calibrated for linear R16G16B16A16F: warm velvet depth, never foggy or muddy
            float nebula = (center_aura * 0.018f + wide_aura * silk * (0.016f + energy * 0.012f) + ptr_glow) * edge_env;
            col += tint * nebula;

            float3 champagne = lerp(accent, float3(1.0f, 0.94f, 0.82f), 0.42f);
            float3 mineral   = lerp(secondary, float3(0.85f, 0.95f, 0.92f), 0.28f);
            col += (mineral * d_far + champagne * (d_mid + d_near)) * dust_mask * 0.95f;
        }
        else
        {
            float wash = saturate((center_aura * 0.04f + wide_aura * silk * 0.06f + ptr_glow * 1.2f) * edge_env);
            col = lerp(col, tint, wash);
            float paper_dust = saturate((d_far * 0.5f + d_mid * 0.75f + d_near) * dust_mask * 0.55f);
            col = lerp(col, lerp(accent, secondary, 0.35f), paper_dust);
        }
    }

    return float4(max(col, 0.0f), 1.0f);
}
