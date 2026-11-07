// fx.hpp — tiny shared effects core for the Cat (.meow) and Molten rocks (.lava) themes.
// Pure maths, no Cocos / ImGui calls: panel.cpp (Cocos) and imgui_ui.cpp (ImGui) both use it and only
// do the drawing. Hard caps keep it cheap: 8 click paws + 16 embers alive at once, nothing is allocated.
//
// Coordinates here are LOCAL to the menu rectangle: y points UP. (ImGui converts with y = bottom - y.)
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "themes.hpp"

namespace cky::fx {
    enum class Kind { None, Cat, Lava, Matrix, Cyber };

    // the theme index is found by its macro extension, so reordering themes.hpp never breaks this
    inline int indexOfExt(char const* ext) {
        for (int i = 0; i < ThemeCount; ++i) if (std::strcmp(kThemeDefs[i].ext, ext) == 0) return i;
        return -1;
    }
    inline Kind kindOf(int theme) {
        static int const cat = indexOfExt(".meow");
        static int const lava = indexOfExt(".lava");
        static int const matrix = indexOfExt(".glitch");
        static int const cyber = indexOfExt(".tech");
        if (theme == cat) return Kind::Cat;
        if (theme == lava) return Kind::Lava;
        if (theme == matrix) return Kind::Matrix;
        if (theme == cyber) return Kind::Cyber;
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
    // cap / rate / lifeScale default to the original look (16 alive, ~9 per second). The ImGui menu and the
    // in-level HUD raise them to get more embers; the array is a fixed 64, nothing is ever allocated.
    struct EmberPool {
        static constexpr int N = 64;
        Particle p[N];
        float acc = 0.f;
        int   cap = 16;            // how many may be alive at once (<= N)
        float rate = 9.f;          // embers spawned per second
        float lifeScale = 1.f;     // multiplies the life time

        void clear() { for (auto& q : p) q.alive = false; acc = 0.f; }
        int  alive() const { int n = 0; for (auto const& q : p) n += q.alive ? 1 : 0; return n; }

        void step(float dt, float unit, Rect const& b) {
            int const lim = std::clamp(cap, 1, N);
            for (auto& q : p) {
                if (!q.alive) continue;
                q.age += dt;
                q.y += q.vy * dt;
                q.x += (q.vx + std::sin(q.age * 5.f + q.phase) * 9.f * unit) * dt;
                if (q.age >= q.life || !inside(b, q.x, q.y, q.size)) q.alive = false;
            }
            acc += dt * rate;
            while (acc >= 1.f) {
                acc -= 1.f;
                Particle* s = nullptr;
                for (int i = 0; i < lim; ++i) if (!p[i].alive) { s = &p[i]; break; }
                if (!s) { acc = 0.f; break; }
                s->size = rf(1.1f, 2.4f) * unit;
                s->x = rf(b.x0 + s->size + 2.f, b.x1 - s->size - 2.f);
                s->y = b.y0 + s->size + 1.f;                  // spawns on the bottom edge, inside
                s->vx = rf(-6.f, 6.f) * unit;
                s->vy = rf(26.f, 58.f) * unit;
                s->age = 0.f;
                s->life = rf(1.6f, 3.2f) * lifeScale;
                s->phase = rf(0.f, 6.28f);
                s->alive = true;
            }
        }
        static float alphaOf(Particle const& q) {
            float t = q.age / q.life;
            return clamp01(t * 10.f) * (1.f - t);
        }
    };


    // =====================================================================================================
    // Matrix (.glitch) + Cyber (.tech): pure maths, no Cocos / ImGui calls (panel.cpp, imgui_ui.cpp and
    // icon.cpp only draw). Everything is fixed-size: RainSim is 24 columns x 7 cells, there are no
    // allocations, no vectors, no heap use per frame, so nothing can leak and the cost stays flat.
    // =====================================================================================================
    inline uint32_t hash32(uint32_t a, uint32_t b) {
        a ^= b * 0x9E3779B1u; a ^= a >> 15; a *= 0x85EBCA77u; a ^= a >> 13; a *= 0xC2B2AE3Du; a ^= a >> 16;
        return a;
    }

    // ---- Molten rocks: lava cracks + broken obsidian border (shared by the HUD and the ImGui menu) ----
    // crackNet: a deterministic network of lava cracks inside a w x h rectangle. Local coordinates, y UP.
    // Calls f(x0, y0, x1, y1, isBranch) once per segment. Same seed = same cracks every frame, so nothing is
    // stored. Cost is flat: at most 14 cracks x 7 segments (+ a few short branches). density scales the count.
    template <class F>
    inline void crackNet(float w, float h, uint32_t seed, float density, F&& f) {
        if (w < 12.f || h < 12.f) return;
        int mains = std::clamp(static_cast<int>(w * h / 19600.f * density + 0.5f), 2, 14);
        float seg = std::clamp(std::min(w, h) * 0.2f, 7.f, 30.f);
        int steps = std::clamp(static_cast<int>(std::max(w, h) / seg * 0.6f), 3, 7);
        for (int i = 0; i < mains; ++i) {
            uint32_t s = hash32(seed, static_cast<uint32_t>(i) * 977u + 13u);
            auto r = [&s](float a, float b) {
                s = hash32(s, 0x51EDu);
                return a + (b - a) * static_cast<float>(s & 0xFFFFu) / 65535.f;
            };
            float x, y, ang, side = r(0.f, 1.f);
            if (side < 0.62f)      { x = r(0.04f, 0.96f) * w; y = 0.f; ang = 1.5708f + r(-0.7f, 0.7f); }   // from the bottom edge
            else if (side < 0.81f) { x = 0.f; y = r(0.f, 0.7f) * h;    ang = r(-0.5f, 0.5f); }             // from the left edge
            else                   { x = w;   y = r(0.f, 0.7f) * h;    ang = 3.14159f + r(-0.5f, 0.5f); }  // from the right edge
            for (int k = 0; k < steps; ++k) {
                ang += r(-0.75f, 0.75f);
                float len = seg * r(0.7f, 1.3f);
                float nx = x + std::cos(ang) * len, ny = y + std::sin(ang) * len;
                float t = 1.f;
                if (nx < 0.f) t = std::min(t, (0.f - x) / (nx - x));
                if (nx > w)   t = std::min(t, (w - x) / (nx - x));
                if (ny < 0.f) t = std::min(t, (0.f - y) / (ny - y));
                if (ny > h)   t = std::min(t, (h - y) / (ny - y));
                if (t <= 0.02f) break;
                bool stop = t < 1.f;
                if (stop) { nx = x + (nx - x) * t; ny = y + (ny - y) * t; }
                f(x, y, nx, ny, false);
                if (!stop && r(0.f, 1.f) < 0.32f) {
                    float ba = ang + (r(0.f, 1.f) < 0.5f ? -1.f : 1.f) * r(0.6f, 1.1f);
                    float bl = seg * r(0.5f, 1.f);
                    float bx = nx + std::cos(ba) * bl, by = ny + std::sin(ba) * bl;
                    if (bx >= 0.f && bx <= w && by >= 0.f && by <= h) f(nx, ny, bx, by, true);
                }
                if (stop) break;
                x = nx; y = ny;
            }
        }
    }

    // brokenEdge: splits the edge (x0,y0)-(x1,y1) into pieces of random length with small random gaps between
    // them. Draw the pieces dark (obsidian) over a glowing line and the gaps show lava glowing through.
    // Calls f(sx, sy, ex, ey) per piece. Deterministic per seed.
    template <class F>
    inline void brokenEdge(float x0, float y0, float x1, float y1, uint32_t seed,
                           float minLen, float maxLen, float minGap, float maxGap, F&& f) {
        float dx = x1 - x0, dy = y1 - y0;
        float L = std::sqrt(dx * dx + dy * dy);
        if (L < 1e-3f) return;
        uint32_t s = hash32(seed, 0xB0DEu);
        auto r = [&s](float a, float b) {
            s = hash32(s, 0x7A11u);
            return a + (b - a) * static_cast<float>(s & 0xFFFFu) / 65535.f;
        };
        float pos = 0.f;
        for (int guard = 0; pos < L && guard < 400; ++guard) {
            float e = std::min(L, pos + r(minLen, maxLen));
            f(x0 + dx * pos / L, y0 + dy * pos / L, x0 + dx * e / L, y0 + dy * e / L);
            pos = e + r(minGap, maxGap);
        }
    }

    // characters the GD font (bigFont.fnt) and the ImGui default font both have
    inline constexpr char kScrambleSet[] = "01X#%&@$?/=+*";
    inline constexpr char kRainGlyphs[]  = "01<>/|=+*:";          // ImGui rain only (Cocos rain draws 0/1 shapes)
    inline constexpr char kBinOn[]  = "1 0 1";                    // Matrix toggle ON  (green)
    inline constexpr char kBinOff[] = "0 X 0";                    // Matrix toggle OFF (corrupted red)
    inline constexpr float kRainAlpha = 0.2f;

    // ---- digital rain: columns of falling cells, head at the bottom of each trail. Coordinates: y UP. ----
    struct RainSim {
        static constexpr int MAXC = 24, TRAIL = 7;
        struct Col { float head = 0.f, speed = 0.f; uint32_t seed = 0; };
        Col c[MAXC];
        int n = 0;
        bool ready = false;
        float epochT = 0.f;
        uint32_t epoch = 0;

        void clear() { ready = false; }

        // unit = height of one cell in px. Columns never get closer than 1.3 cells.
        void step(float dt, float unit, Rect const& b) {
            float w = b.x1 - b.x0, h = b.y1 - b.y0;
            if (w < unit * 2.f || h < unit * 2.f) { n = 0; return; }
            n = std::clamp(static_cast<int>(w / (unit * 1.3f)), 1, MAXC);
            if (!ready) {
                for (int i = 0; i < MAXC; ++i) {
                    c[i].head = rf(b.y0, b.y1 + unit * TRAIL);
                    c[i].speed = rf(4.f, 10.f);
                    c[i].seed = rnd();
                }
                ready = true;
            }
            epochT += dt;
            while (epochT >= 0.12f) { epochT -= 0.12f; ++epoch; }
            for (int i = 0; i < n; ++i) {
                c[i].head -= c[i].speed * unit * dt;
                if (c[i].head < b.y0 - unit) {                       // fell out of the bottom: restart above the top
                    c[i].head = b.y1 + rf(0.f, unit * TRAIL);
                    c[i].speed = rf(4.f, 10.f);
                    c[i].seed = rnd();
                }
            }
        }
        float colX(int i, Rect const& b) const { return b.x0 + (static_cast<float>(i) + 0.5f) * (b.x1 - b.x0) / static_cast<float>(n); }
        // cell k of a column sits k cells above the head. k = 0 is the bright head.
        static float cellAlpha(int k) { return k == 0 ? 1.f : 0.8f * (1.f - static_cast<float>(k) / TRAIL); }
        // glyph id of a cell; every cell flips at its own phase (the head flips fastest)
        uint32_t glyph(int col, int k) const {
            uint32_t phase = hash32(c[col].seed, static_cast<uint32_t>(k)) % 6u;
            uint32_t e = k == 0 ? epoch : (epoch + phase) / 6u;
            return hash32(c[col].seed + static_cast<uint32_t>(k) * 7919u, e);
        }
        bool bit(int col, int k) const { return ((glyph(col, k) >> 7) & 1u) != 0; }
    };

    // ---- text glitch (Matrix) ----
    // Should this label have one character swapped right now? Decided per label id in 1/12 s buckets, so a
    // label is glitched for ~0.08 s and, with oneIn = 110, roughly 2 labels per second glitch across a menu.
    struct Glitch { bool on = false; int idx = 0; char ch = '#'; bool red = false; };
    inline Glitch glitchFor(uint32_t id, double time, size_t len, uint32_t oneIn = 110) {
        Glitch g;
        if (len == 0) return g;
        uint32_t h = hash32(id, static_cast<uint32_t>(time * 12.0));
        if (h % oneIn != 0) return g;
        g.on = true;
        g.idx = static_cast<int>((h >> 8) % len);
        g.ch = kScrambleSet[(h >> 12) % (sizeof(kScrambleSet) - 1)];
        g.red = ((h >> 20) & 1u) != 0;
        return g;
    }

    // Matrix toggle click: random characters scroll past, then lock in one by one (left to right).
    // t = seconds since the click. Writes the text to show into out; returns true once every char is locked.
    inline bool scramble(char const* target, float t, uint32_t epoch, char* out, size_t cap) {
        size_t len = std::strlen(target);
        if (len + 1 > cap) len = cap - 1;
        bool done = true;
        for (size_t i = 0; i < len; ++i) {
            char ch = target[i];
            if (ch == ' ') { out[i] = ' '; continue; }
            if (t >= 0.12f + 0.09f * static_cast<float>(i)) { out[i] = ch; continue; }
            done = false;
            out[i] = kScrambleSet[hash32(epoch, static_cast<uint32_t>(i) + 1u) % (sizeof(kScrambleSet) - 1)];
        }
        out[len] = 0;
        return done;
    }

    // ---- Cyber ----
    // laser position 0..1..0 (slow, eased at both ends). period = seconds for one way.
    inline float scanPos(double t, float period = 3.8f) {
        float u = static_cast<float>(std::fmod(t / static_cast<double>(period), 2.0));
        if (u > 1.f) u = 2.f - u;
        return u * u * (3.f - 2.f * u);
    }
    // 1 right under the laser, falling to 0 at `band` px away
    inline float laserHit(float y, float laserY, float band) { return clamp01(1.f - std::fabs(y - laserY) / band); }
    inline float pulse01(double t, float hz) { return 0.5f + 0.5f * static_cast<float>(std::sin(6.28318530718 * hz * t)); }

    // elongated hexagon (the Cyber switch), 6 points, y up, counter-clockwise from bottom-left
    inline void hexagon(float x0, float y0, float x1, float y1, float c, float (&out)[6][2]) {
        float cy = (y0 + y1) * 0.5f;
        float pts[6][2] = { { x0 + c, y0 }, { x1 - c, y0 }, { x1, cy }, { x1 - c, y1 }, { x0 + c, y1 }, { x0, cy } };
        std::memcpy(out, pts, sizeof(pts));
    }

    // chamfered ("hexagonal") corner polygon, 8 points, y up, counter-clockwise from bottom-left chamfer
    inline void chamfer(float x0, float y0, float x1, float y1, float c, float (&out)[8][2]) {
        float pts[8][2] = {
            { x0 + c, y0 }, { x1 - c, y0 }, { x1, y0 + c }, { x1, y1 - c },
            { x1 - c, y1 }, { x0 + c, y1 }, { x0, y1 - c }, { x0, y0 + c },
        };
        std::memcpy(out, pts, sizeof(pts));
    }
}
