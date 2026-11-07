// fx.hpp — tiny shared effects core for the Cat (.meow) and Molten rocks (.lava) themes.
// Pure maths, no Cocos / ImGui calls: panel.cpp (Cocos) and imgui_ui.cpp (ImGui) both use it and only
// do the drawing. Hard caps keep it cheap: 8 click paws + 16 embers alive at once, nothing is allocated.
//
// Coordinates here are LOCAL to the menu rectangle: y points UP. (ImGui converts with y = bottom - y.)
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "themes.hpp"

namespace cky::fx {
    enum class Kind { None, Cat, Lava };

    // the theme index is found by its macro extension, so reordering themes.hpp never breaks this
    inline int indexOfExt(char const* ext) {
        for (int i = 0; i < ThemeCount; ++i) if (std::strcmp(kThemeDefs[i].ext, ext) == 0) return i;
        return -1;
    }
    inline Kind kindOf(int theme) {
        static int const cat = indexOfExt(".meow");
        static int const lava = indexOfExt(".lava");
        if (theme == cat) return Kind::Cat;
        if (theme == lava) return Kind::Lava;
        return Kind::None;
    }

    // ---- tiny xorshift RNG (no <random>, no allocations) ----
    inline uint32_t rnd() {
        static uint32_t s = 0x9E3779B9u;
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    inline float rf(float a, float b) { return a + (b - a) * static_cast<float>(rnd() & 0xFFFFu) / 65535.f; }
    inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
    inline float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

    struct Rect { float x0, y0, x1, y1; };       // y up
    struct Col { float r, g, b; };

    // ---- palette ----
    inline constexpr Col kPeach     = { 1.00f, 0.71f, 0.63f };   // paw colour (peach pink)
    inline constexpr Col kPeachDeep = { 1.00f, 0.55f, 0.58f };   // paw in the toggle box
    inline constexpr Col kPinkWhite = { 1.00f, 0.957f, 0.969f }; // near-white pink background

    // ember colour over its life: yellow -> orange -> dull red
    inline Col emberColor(float t) {
        t = clamp01(t);
        if (t < 0.5f) { float k = t * 2.f; return { 1.f, lerp(0.85f, 0.45f, k), lerp(0.30f, 0.08f, k) }; }
        float k = (t - 0.5f) * 2.f;
        return { lerp(1.f, 0.70f, k), lerp(0.45f, 0.12f, k), lerp(0.08f, 0.04f, k) };
    }

    // ---- paw shape: 1 pad + 4 toes, none overlapping (so a see-through paw has no darker spots) ----
    // calls f(x, y, radius) five times. s = paw size, rot in radians.
    template <class F>
    inline void pawDots(float cx, float cy, float s, float rot, F&& f) {
        static constexpr float off[5][3] = {
            {  0.00f, -0.15f, 0.42f },   // pad
            { -0.50f,  0.28f, 0.19f },   // toes
            { -0.17f,  0.52f, 0.19f },
            {  0.17f,  0.52f, 0.19f },
            {  0.50f,  0.28f, 0.19f },
        };
        float c = std::cos(rot), sn = std::sin(rot);
        for (auto const& o : off) {
            float ox = o[0] * s, oy = o[1] * s;
            f(cx + ox * c - oy * sn, cy + ox * sn + oy * c, o[2] * s);
        }
    }
    inline float pawExtent(float s) { return s * 0.80f; }   // safe radius of a whole paw

    // static background paws, positions are 0..1 of the menu rectangle. Different sizes, scattered.
    struct BgPaw { float nx, ny, size, rotDeg; };
    inline constexpr BgPaw kBgPaws[] = {
        { 0.07f, 0.88f, 13.f, -20.f }, { 0.22f, 0.62f,  9.f,  25.f }, { 0.12f, 0.30f, 15.f,  10.f },
        { 0.30f, 0.10f,  8.f, -35.f }, { 0.38f, 0.82f, 11.f,  15.f }, { 0.52f, 0.50f, 17.f, -10.f },
        { 0.60f, 0.92f,  8.f,  30.f }, { 0.72f, 0.22f, 14.f,  20.f }, { 0.85f, 0.70f, 12.f, -25.f },
        { 0.93f, 0.38f,  9.f,  35.f }, { 0.80f, 0.07f,  7.f, -15.f }, { 0.46f, 0.28f,  7.f,  40.f },
        { 0.94f, 0.93f, 10.f,   0.f },
    };
    inline constexpr float kBgPawAlpha = 0.4f;

    struct Particle {
        float x = 0, y = 0, vx = 0, vy = 0;
        float age = 0, life = 0, size = 0, rot = 0, spin = 0, phase = 0;
        bool  alive = false;
    };

    // does a particle of radius r still sit fully inside the rectangle? (the "clip": never render outside)
    inline bool inside(Rect const& b, float x, float y, float r) {
        return x - r >= b.x0 && x + r <= b.x1 && y - r >= b.y0 && y + r <= b.y1;
    }

    // ---- click paws (Cat): small, fade out, float up ----
    struct PawPool {
        static constexpr int N = 8;
        Particle p[N];

        void clear() { for (auto& q : p) q.alive = false; }
        int  alive() const { int n = 0; for (auto const& q : p) n += q.alive ? 1 : 0; return n; }

        // x,y = click position. The paw is pushed inside the rectangle so it never spawns outside it.
        void spawn(float x, float y, float unit, Rect const& b) {
            Particle* s = nullptr;
            for (auto& q : p) if (!q.alive) { s = &q; break; }
            if (!s) return;                                  // pool full: drop it, keeps the cost flat
            float size = rf(6.f, 10.f) * unit;
            float e = pawExtent(size) + 1.f;
            if (b.x1 - b.x0 < e * 2.f || b.y1 - b.y0 < e * 2.f) return;
            s->size = size;
            s->x = std::clamp(x, b.x0 + e, b.x1 - e);
            s->y = std::clamp(y, b.y0 + e, b.y1 - e);
            s->vx = 0.f;
            s->vy = rf(16.f, 28.f) * unit;
            s->age = 0.f;
            s->life = rf(0.9f, 1.4f);
            s->rot = rf(-0.5f, 0.5f);
            s->spin = rf(-0.8f, 0.8f);
            s->phase = rf(0.f, 6.28f);
            s->alive = true;
        }
        void step(float dt, float unit, Rect const& b) {
            for (auto& q : p) {
                if (!q.alive) continue;
                q.age += dt;
                q.y += q.vy * dt;
                q.x += std::sin(q.age * 4.f + q.phase) * 6.f * unit * dt;
                q.rot += q.spin * dt;
                if (q.age >= q.life || !inside(b, q.x, q.y, pawExtent(q.size))) q.alive = false;
            }
        }
        static float alphaOf(Particle const& q) {            // quick fade in, then fade out
            float t = q.age / q.life;
            return clamp01(t * 8.f) * (1.f - t) * 0.9f;
        }
    };

    // ---- embers (Lava): tiny, rise from the bottom edge ----
    struct EmberPool {
        static constexpr int N = 16;
        Particle p[N];
        float acc = 0.f;

        void clear() { for (auto& q : p) q.alive = false; acc = 0.f; }
        int  alive() const { int n = 0; for (auto const& q : p) n += q.alive ? 1 : 0; return n; }

        void step(float dt, float unit, Rect const& b) {
            for (auto& q : p) {
                if (!q.alive) continue;
                q.age += dt;
                q.y += q.vy * dt;
                q.x += (q.vx + std::sin(q.age * 5.f + q.phase) * 9.f * unit) * dt;
                if (q.age >= q.life || !inside(b, q.x, q.y, q.size)) q.alive = false;
            }
            acc += dt * 9.f;                                  // ~9 embers per second
            while (acc >= 1.f) {
                acc -= 1.f;
                Particle* s = nullptr;
                for (auto& q : p) if (!q.alive) { s = &q; break; }
                if (!s) { acc = 0.f; break; }
                s->size = rf(1.1f, 2.4f) * unit;
                s->x = rf(b.x0 + s->size + 2.f, b.x1 - s->size - 2.f);
                s->y = b.y0 + s->size + 1.f;                  // spawns on the bottom edge, inside
                s->vx = rf(-6.f, 6.f) * unit;
                s->vy = rf(26.f, 58.f) * unit;
                s->age = 0.f;
                s->life = rf(1.6f, 3.2f);
                s->phase = rf(0.f, 6.28f);
                s->alive = true;
            }
        }
        static float alphaOf(Particle const& q) {
            float t = q.age / q.life;
            return clamp01(t * 10.f) * (1.f - t);
        }
    };

    // chamfered ("hexagonal") corner polygon, 8 points, y up, counter-clockwise from bottom-left chamfer
    inline void chamfer(float x0, float y0, float x1, float y1, float c, float (&out)[8][2]) {
        float pts[8][2] = {
            { x0 + c, y0 }, { x1 - c, y0 }, { x1, y0 + c }, { x1, y1 - c },
            { x1 - c, y1 }, { x0 + c, y1 }, { x0, y1 - c }, { x0, y0 + c },
        };
        std::memcpy(out, pts, sizeof(pts));
    }
}
