// imgui_ui.cpp
#include "state.hpp"
#include "fx.hpp"

#if __has_include(<imgui-cocos.hpp>)

#include <imgui-cocos.hpp>
#include <Geode/utils/web.hpp>
#include <cstdlib>

namespace {
    bool g_ready = false;

    ImVec4 themeHi(int t) {
        auto const& v = cky::kThemeDefs[t].hi;
        return ImVec4(v[0], v[1], v[2], 1.f);
    }
    ImVec4 themeLo(int t) {
        auto const& v = cky::kThemeDefs[t].lo;
        return ImVec4(v[0], v[1], v[2], 1.f);
    }

    bool wantsImGui() {
        return Mod::get()->getSettingValue<std::string>("ui-mode") == "ImGui";
    }

    // ======================= theme effects: Cat (.meow) + Molten rocks (.lava) =======================
    // Maths (particles, paw shape, caps) is in fx.hpp. Everything here is a few draw-list calls per frame:
    // <= 8 click paws, <= 16 embers, 13 static background paws. Nothing is allocated.
    namespace fx = cky::fx;
    using fx::Kind;
    Kind curKind() { return fx::kindOf(cky::theme); }

    ImU32 U(float r, float g, float b, float a = 1.f) { return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a)); }
    ImU32 U(fx::Col c, float a = 1.f) { return U(c.r, c.g, c.b, a); }
    float lerpf(float a, float b, float t) { return a + (b - a) * t; }

    void imPaw(ImDrawList* dl, float x, float y, float s, float rot, ImU32 col) {
        fx::pawDots(x, y, s, rot, [&](float px, float py, float r) { dl->AddCircleFilled(ImVec2(px, py), r, col, 10); });
    }
    // chamfered ("hexagonal") corners; the 8 points run clockwise on screen, as ImGui wants
    void imChamfer(float x0, float y0, float x1, float y1, float c, ImVec2 (&o)[8]) {
        float f[8][2];
        fx::chamfer(x0, y0, x1, y1, c, f);
        for (int i = 0; i < 8; ++i) o[i] = ImVec2(f[i][0], f[i][1]);
    }

    // heated-metal gradient: white-hot orange at the bottom, deep red-orange at the top
    ImU32 heatAt(float t /*0 top .. 1 bottom*/, float a) {
        return U(lerpf(0.85f, 1.00f, t), lerpf(0.20f, 0.62f, t), lerpf(0.05f, 0.14f, t), a);
    }

    fx::PawPool   g_paws;
    fx::EmberPool g_embers;
    Kind          g_lastKind = Kind::None;
    constexpr float PAW_UNIT = 2.4f, EMBER_UNIT = 2.2f;   // the sim is written in 440x270 "Cocos px"; ImGui is bigger

    // ======================= Matrix (.glitch) + Cyber (.tech) =======================
    // Cost per frame: Matrix = <= 24 x 7 rain characters; Cyber = ~70 grid lines + ~60 glow dots + the laser (3 calls).
    // All state is fixed-size (RainSim) or kept in ImGui's own state storage; nothing is allocated.
    fx::RainSim g_rain;
    float g_laserY = -1e9f;                                // screen y of the Cyber laser this frame (widgets light up near it)
    constexpr float RAIN_UNIT = 15.f;                      // ImGui px per rain cell

    // label text where, now and then, one character swaps to a red or white symbol for a split second (Matrix)
    void mxLabel(ImDrawList* dl, ImVec2 pos, ImU32 col, char const* s, char const* end, ImGuiID id) {
        size_t len = end ? static_cast<size_t>(end - s) : std::strlen(s);
        fx::Glitch g = fx::glitchFor(id, ImGui::GetTime(), len);
        if (!g.on || s[g.idx] == ' ') { dl->AddText(pos, col, s, s + len); return; }
        ImVec2 w0 = ImGui::CalcTextSize(s, s + g.idx);
        ImVec2 w1 = ImGui::CalcTextSize(s + g.idx, s + g.idx + 1);
        char gc[2] = { g.ch, 0 };
        dl->AddText(pos, col, s, s + g.idx);
        dl->AddText(ImVec2(pos.x + w0.x, pos.y), g.red ? U(1.f, 0.2f, 0.2f) : U(1.f, 1.f, 1.f), gc, gc + 1);
        dl->AddText(ImVec2(pos.x + w0.x + w1.x, pos.y), col, s + g.idx + 1, s + len);
    }

    void mxBegin(ImVec2 wp, ImVec2 ws) {
        float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        ImVec2 wmax(wp.x + ws.x, wp.y + ws.y);
        float th = ImGui::GetFrameHeight();
        bg->AddRectFilled(wp, wmax, U(0.f, 0.f, 0.f, 1.f));                              // pure black
        bg->AddRectFilled(wp, ImVec2(wmax.x, wp.y + th), U(0.f, 0.16f, 0.06f, 1.f));     // title bar

        // digital rain, 0.2 opacity, clipped to the window below the title bar
        float areaTop = wp.y + th, areaH = ws.y - th;
        fx::Rect box{ 0.f, 0.f, ws.x, areaH };                                            // local, y up
        g_rain.step(dt, RAIN_UNIT, box);
        bg->PushClipRect(ImVec2(wp.x, areaTop), wmax, true);
        ImFont* font = ImGui::GetFont();
        float fs = RAIN_UNIT * 0.95f;
        for (int i = 0; i < g_rain.n; ++i) {
            float x = wp.x + g_rain.colX(i, box) - fs * 0.25f;
            for (int k = 0; k < fx::RainSim::TRAIL; ++k) {
                float y = g_rain.c[i].head + static_cast<float>(k) * RAIN_UNIT;
                if (y < 0.f || y > areaH) continue;
                float a = fx::RainSim::cellAlpha(k) * fx::kRainAlpha;
                ImU32 col = k == 0 ? U(0.75f, 1.f, 0.8f, a) : U(0.f, 1.f, 0.35f, a);
                char const* gl = &fx::kRainGlyphs[g_rain.glyph(i, k) % (sizeof(fx::kRainGlyphs) - 1)];
                bg->AddText(font, fs, ImVec2(x, areaTop + (areaH - y) - fs * 0.5f), col, gl, gl + 1);
            }
        }
        bg->PopClipRect();

        // green border; once in a while it tears for one frame (red ghost, shoved sideways)
        bg->AddRect(wp, wmax, U(0.f, 0.9f, 0.3f, 1.f), 0.f, 0, 2.f);
        if (fx::rnd() % 90 == 0)
            bg->AddRect(ImVec2(wp.x + fx::rf(-5.f, 5.f), wp.y + fx::rf(-3.f, 3.f)),
                        ImVec2(wmax.x + fx::rf(-5.f, 5.f), wmax.y), U(1.f, 0.15f, 0.15f, 0.8f), 0.f, 0, 2.f);
    }

    void cyBegin(ImVec2 wp, ImVec2 ws) {
        double now = ImGui::GetTime();
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        ImVec2 wmax(wp.x + ws.x, wp.y + ws.y);
        float th = ImGui::GetFrameHeight();
        // deep dark blue -> near black
        bg->AddRectFilledMultiColor(wp, wmax, U(0.02f, 0.05f, 0.12f), U(0.02f, 0.05f, 0.12f), U(0.f, 0.01f, 0.04f), U(0.f, 0.01f, 0.04f));
        bg->AddRectFilled(wp, ImVec2(wmax.x, wp.y + th), U(0.f, 0.14f, 0.22f, 1.f));
        bg->AddLine(ImVec2(wp.x, wp.y + th), ImVec2(wmax.x, wp.y + th), U(0.f, 0.9f, 1.f, 0.8f), 1.5f);

        // subtle glowing grid (slow brightness breathing), clipped under the title bar
        float glow = 0.85f + 0.15f * std::sin(static_cast<float>(now) * 1.6f);
        constexpr float CELL = 30.f;
        float gx0 = wp.x, gy0 = wp.y + th;
        bg->PushClipRect(ImVec2(gx0, gy0), wmax, true);
        int ix = 0;
        for (float x = gx0 + CELL; x < wmax.x; x += CELL, ++ix) {
            bool major = (ix % 4) == 3;
            bg->AddLine(ImVec2(x, gy0), ImVec2(x, wmax.y), U(0.f, 0.85f, 1.f, (major ? 0.15f : 0.065f) * glow), major ? 1.5f : 1.f);
        }
        int iy = 0;
        for (float y = gy0 + CELL; y < wmax.y; y += CELL, ++iy) {
            bool major = (iy % 4) == 3;
            bg->AddLine(ImVec2(gx0, y), ImVec2(wmax.x, y), U(0.f, 0.85f, 1.f, (major ? 0.15f : 0.065f) * glow), major ? 1.5f : 1.f);
        }
        ix = 0;
        for (float x = gx0 + CELL; x < wmax.x; x += CELL, ++ix) {
            if ((ix % 4) != 3) continue;
            iy = 0;
            for (float y = gy0 + CELL; y < wmax.y; y += CELL, ++iy)
                if ((iy % 4) == 3) bg->AddCircleFilled(ImVec2(x, y), 2.2f, U(0.f, 0.9f, 1.f, 0.22f * glow), 8);   // glowing intersections
        }
        bg->PopClipRect();

        // neon border with a soft pulsing halo
        float pulse = fx::pulse01(now, 0.6f);
        bg->AddRect(wp, wmax, U(0.f, 0.9f, 1.f, 0.10f + 0.10f * pulse), 0.f, 0, 8.f);
        bg->AddRect(wp, wmax, U(0.f, 0.95f, 1.f, 0.95f), 0.f, 0, 2.f);

        // laser height this frame; fxEnd draws it, fxButton / the toggles read it to light up
        g_laserY = gy0 + (ws.y - th - 4.f) * fx::scanPos(now);
    }

    // Called right after Begin(): draws the themed window body BEHIND the widgets (background draw list,
    // clipped to the window rect so nothing ever renders outside the menu) and steps the particles.
    void fxBegin(ImVec2 wp, ImVec2 ws) {
        Kind k = curKind();
        if (k != g_lastKind) { g_paws.clear(); g_embers.clear(); g_rain.clear(); g_lastKind = k; }
        if (k == Kind::None) return;
        if (k == Kind::Matrix) { mxBegin(wp, ws); return; }
        if (k == Kind::Cyber)  { cyBegin(wp, ws);  return; }

        float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
        double now = ImGui::GetTime();
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        ImVec2 wmax(wp.x + ws.x, wp.y + ws.y);
        fx::Rect box{ 4.f, 4.f, ws.x - 4.f, ws.y - 4.f };           // local, y up
        float rounding = ImGui::GetStyle().WindowRounding;

        if (k == Kind::Cat) {
            // little ears on the top corners
            float e = std::min(ws.x, 900.f) * 0.035f + 12.f;
            ImU32 ear = U(fx::kPeach), earIn = U(1.f, 0.86f, 0.88f);
            bg->AddTriangleFilled(ImVec2(wp.x + rounding * 0.5f, wp.y + 4.f), ImVec2(wp.x + rounding * 0.5f + e * 0.2f, wp.y - e),
                                  ImVec2(wp.x + rounding * 0.5f + e * 1.5f, wp.y + 4.f), ear);
            bg->AddTriangleFilled(ImVec2(wp.x + ws.x - rounding * 0.5f, wp.y + 4.f), ImVec2(wp.x + ws.x - rounding * 0.5f - e * 0.2f, wp.y - e),
                                  ImVec2(wp.x + ws.x - rounding * 0.5f - e * 1.5f, wp.y + 4.f), ear);
            bg->AddTriangleFilled(ImVec2(wp.x + rounding * 0.5f + e * 0.25f, wp.y + 2.f), ImVec2(wp.x + rounding * 0.5f + e * 0.28f, wp.y - e * 0.6f),
                                  ImVec2(wp.x + rounding * 0.5f + e * 0.95f, wp.y + 2.f), earIn);
            bg->AddTriangleFilled(ImVec2(wp.x + ws.x - rounding * 0.5f - e * 0.25f, wp.y + 2.f), ImVec2(wp.x + ws.x - rounding * 0.5f - e * 0.28f, wp.y - e * 0.6f),
                                  ImVec2(wp.x + ws.x - rounding * 0.5f - e * 0.95f, wp.y + 2.f), earIn);

            bg->AddRectFilled(wp, wmax, U(fx::kPinkWhite), rounding);             // near-white pink body

            // scattered background paws: 0.4 opacity, different sizes, clipped to the window
            bg->PushClipRect(wp, wmax, true);
            float sc = std::clamp(ws.y / 270.f, 1.f, 3.f);
            ImU32 pawCol = U(fx::kPeach, fx::kBgPawAlpha);
            for (auto const& b : fx::kBgPaws) {
                float s2 = b.size * sc;
                float ex = fx::pawExtent(s2) + 4.f;
                float x = std::clamp(b.nx * ws.x, ex, ws.x - ex);
                float y = std::clamp(b.ny * ws.y, ex, ws.y - ex);
                imPaw(bg, wp.x + x, wp.y + ws.y - y, s2, b.rotDeg * 3.14159265f / 180.f, pawCol);
            }
            bg->PopClipRect();
            bg->AddRect(wp, wmax, U(fx::kPeach), rounding, 0, 3.f);

            // click anywhere inside the menu -> a small paw floats up and fades
            if (ImGui::IsMouseClicked(0)) {
                ImVec2 m = ImGui::GetMousePos();
                if (m.x >= wp.x && m.x <= wmax.x && m.y >= wp.y && m.y <= wmax.y)
                    g_paws.spawn(m.x - wp.x, wmax.y - m.y, PAW_UNIT, box);
            }
            g_paws.step(dt, PAW_UNIT, box);
        } else {
            // ---- Molten rocks: chamfered window, heated-metal border, rising embers ----
            float c = std::clamp(ws.y * 0.04f, 12.f, 26.f);
            float th = ImGui::GetFrameHeight();
            ImVec2 body[8];
            imChamfer(wp.x, wp.y, wmax.x, wmax.y, c, body);
            bg->AddConvexPolyFilled(body, 8, U(0.11f, 0.063f, 0.047f, 0.97f));
            auto const& lo = cky::kThemeDefs[std::clamp(cky::theme, 0, cky::ThemeCount - 1)].lo;
            ImVec2 bar[6] = { ImVec2(wp.x + c, wp.y), ImVec2(wmax.x - c, wp.y), ImVec2(wmax.x, wp.y + c),
                              ImVec2(wmax.x, wp.y + th), ImVec2(wp.x, wp.y + th), ImVec2(wp.x, wp.y + c) };
            bg->AddConvexPolyFilled(bar, 6, U(lo[0], lo[1], lo[2], 1.f));

            // embers (behind the widgets, clipped to the window)
            g_embers.step(dt, EMBER_UNIT, box);
            bg->PushClipRect(wp, wmax, true);
            for (auto const& q : g_embers.p) {
                if (!q.alive) continue;
                float a = fx::EmberPool::alphaOf(q);
                fx::Col col = fx::emberColor(q.age / q.life);
                ImVec2 pos(wp.x + q.x, wmax.y - q.y);
                if (q.size > 3.4f) bg->AddCircleFilled(pos, q.size * 2.2f, U(col, a * 0.18f), 8);   // soft glow
                bg->AddCircleFilled(pos, q.size, U(col, a), 6);
            }
            bg->PopClipRect();

            // glowing border: layered strokes, colour by height, slow pulse. ~40 line calls.
            float pulse = 0.85f + 0.15f * std::sin(static_cast<float>(now) * 2.2f);
            ImVec2 p[8];
            imChamfer(wp.x + 2.f, wp.y + 2.f, wmax.x - 2.f, wmax.y - 2.f, c, p);
            auto stroke = [&](float width, float alpha) {
                for (int i = 0; i < 8; ++i) {
                    ImVec2 a = p[i], b = p[(i + 1) % 8];
                    int n = std::max(1, static_cast<int>(std::ceil(std::fabs(b.y - a.y) / 60.f)));
                    for (int j = 0; j < n; ++j) {
                        ImVec2 s0(lerpf(a.x, b.x, float(j) / n), lerpf(a.y, b.y, float(j) / n));
                        ImVec2 s1(lerpf(a.x, b.x, float(j + 1) / n), lerpf(a.y, b.y, float(j + 1) / n));
                        float t = std::clamp(((s0.y + s1.y) * 0.5f - wp.y) / ws.y, 0.f, 1.f);
                        bg->AddLine(s0, s1, heatAt(t, alpha), width);
                    }
                    float t = std::clamp((b.y - wp.y) / ws.y, 0.f, 1.f);
                    bg->AddCircleFilled(b, width * 0.5f, heatAt(t, alpha), 8);     // closes the corner joints
                }
            };
            stroke(9.f, 0.16f * pulse);
            stroke(5.f, 0.38f * pulse);
            stroke(2.5f, 0.95f);
        }
    }

    // Called just before End(): click paws float on top of the widgets, still clipped to the window.
    void fxEnd(ImVec2 wp, ImVec2 ws) {
        if (curKind() == Kind::Cyber) {
            // the scanning laser: bright cyan line + a soft band each side, on top of the widgets, clipped to the window.
            // 3 draw calls. Whatever it crosses is lit by the band (and the toggles / buttons brighten themselves).
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            ImVec2 wmax(wp.x + ws.x, wp.y + ws.y);
            fg->PushClipRect(ImVec2(wp.x, wp.y + ImGui::GetFrameHeight()), wmax, true);
            float y = g_laserY, band = 26.f;
            fg->AddRectFilledMultiColor(ImVec2(wp.x, y - band), ImVec2(wmax.x, y),
                U(0.f, 1.f, 1.f, 0.f), U(0.f, 1.f, 1.f, 0.f), U(0.f, 1.f, 1.f, 0.16f), U(0.f, 1.f, 1.f, 0.16f));
            fg->AddRectFilledMultiColor(ImVec2(wp.x, y), ImVec2(wmax.x, y + band),
                U(0.f, 1.f, 1.f, 0.16f), U(0.f, 1.f, 1.f, 0.16f), U(0.f, 1.f, 1.f, 0.f), U(0.f, 1.f, 1.f, 0.f));
            fg->AddLine(ImVec2(wp.x, y), ImVec2(wmax.x, y), U(0.5f, 1.f, 1.f, 0.95f), 2.f);
            fg->PopClipRect();
            return;
        }
        if (curKind() != Kind::Cat || g_paws.alive() == 0) return;
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        ImVec2 wmax(wp.x + ws.x, wp.y + ws.y);
        fg->PushClipRect(wp, wmax, true);
        for (auto const& q : g_paws.p) {
            if (!q.alive) continue;
            imPaw(fg, wp.x + q.x, wmax.y - q.y, q.size, q.rot, U(fx::kPeachDeep, fx::PawPool::alphaOf(q)));
        }
        fg->PopClipRect();
    }

    // Button. Cat: springy hover bounce (the button pops up, overshoots and settles, squashes when pressed).
    // Lava: chamfered hexagonal corners + orange rim. Every other theme: the stock ImGui button.
    bool fxButton(char const* label) {
        Kind k = curKind();
        if (k == Kind::None) return ImGui::Button(label);
        ImGuiStyle& st = ImGui::GetStyle();
        ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
        ImVec2 sz(ts.x + st.FramePadding.x * 2.f, ts.y + st.FramePadding.y * 2.f);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGuiID id = ImGui::GetID(label);
        bool pressed = ImGui::InvisibleButton(label, sz);
        bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
        ImVec2 c(p.x + sz.x * 0.5f, p.y + sz.y * 0.5f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 fill = ImGui::GetColorU32(act ? ImGuiCol_ButtonActive : hov ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
        ImU32 txt = ImGui::GetColorU32(ImGuiCol_Text);
        char const* end = std::strstr(label, "##");

        if (k == Kind::Cat) {
            ImGuiStorage* S = ImGui::GetStateStorage();
            float t = S->GetFloat(id, -1.f);                      // seconds since the hover began, -1 = not hovered
            t = hov ? (t < 0.f ? 0.f : t + ImGui::GetIO().DeltaTime) : -1.f;
            S->SetFloat(id, t);
            float s = 1.f;
            if (t >= 0.f) s = 1.03f + 0.10f * std::exp(-6.f * t) * std::sin(15.f * t);
            if (act) s = 0.95f;
            ImVec2 h(sz.x * 0.5f * s, sz.y * 0.5f * s);
            float r = st.FrameRounding + 4.f;
            dl->AddRectFilled(ImVec2(c.x - h.x, c.y - h.y), ImVec2(c.x + h.x, c.y + h.y), fill, r);
            dl->AddRect(ImVec2(c.x - h.x, c.y - h.y), ImVec2(c.x + h.x, c.y + h.y), U(1.f, 0.62f, 0.60f, 0.9f), r, 0, 2.f);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * s,
                        ImVec2(c.x - ts.x * 0.5f * s, c.y - ts.y * 0.5f * s), txt, label, end);
        } else if (k == Kind::Cyber) {
            // chamfered sci-fi corners + glowing cyan outline; brighter while the laser passes over it
            float li = fx::laserHit(c.y, g_laserY, sz.y * 1.2f);
            ImVec2 pts[8];
            imChamfer(p.x, p.y, p.x + sz.x, p.y + sz.y, sz.y * 0.28f, pts);
            dl->AddConvexPolyFilled(pts, 8, act ? U(0.f, 0.35f, 0.5f) : hov ? U(0.f, 0.22f, 0.34f) : U(0.01f, 0.08f, 0.15f));
            if (li > 0.f) dl->AddConvexPolyFilled(pts, 8, U(0.f, 1.f, 1.f, 0.18f * li));
            dl->AddPolyline(pts, 8, U(0.f, 0.9f, 1.f, 0.16f + 0.16f * li + (hov || act ? 0.12f : 0.f)), ImDrawFlags_Closed, 6.f);   // glow
            dl->AddPolyline(pts, 8, U(lerpf(0.f, 0.7f, li), 0.95f, 1.f, hov || act ? 1.f : 0.88f), ImDrawFlags_Closed, 2.f);        // outline
            dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), U(0.82f, 1.f, 1.f), label, end);
        } else if (k == Kind::Matrix) {
            // flat black + green border. While hovered / pressed the border violently glitches: new random shoves,
            // a red and a white ghost, every frame (65% of frames), so it looks like it tears for a frame at a time
            ImVec2 a(p.x, p.y), b(p.x + sz.x, p.y + sz.y);
            dl->AddRectFilled(a, b, hov || act ? U(0.f, 0.16f, 0.06f) : U(0.f, 0.f, 0.f));
            ImU32 green = U(0.f, 0.95f, 0.35f);
            if ((hov || act) && fx::rnd() % 100 < 65u) {
                int n = 2 + static_cast<int>(fx::rnd() % 2u);
                for (int j = 0; j < n; ++j) {
                    float dx = fx::rf(-5.f, 5.f), dy = fx::rf(-3.f, 3.f);
                    ImU32 col = j == 0 ? U(1.f, 0.15f, 0.15f, 0.9f) : j == 1 ? U(1.f, 1.f, 1.f, 0.8f) : green;
                    dl->AddRect(ImVec2(a.x + dx, a.y + dy), ImVec2(b.x + dx, b.y + dy), col, 0.f, 0, 1.5f);
                }
            } else {
                dl->AddRect(a, b, green, 0.f, 0, 1.5f);
            }
            mxLabel(dl, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), U(0.78f, 1.f, 0.82f), label, end, id);
        } else {
            ImVec2 pts[8];
            imChamfer(p.x, p.y, p.x + sz.x, p.y + sz.y, sz.y * 0.28f, pts);
            dl->AddConvexPolyFilled(pts, 8, fill);
            dl->AddPolyline(pts, 8, U(1.f, 0.55f, 0.16f, hov || act ? 1.f : 0.8f), ImDrawFlags_Closed, 2.f);
            dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), txt, label, end);
        }
        return pressed;
    }

    // Matrix toggle: no checkbox. A block of binary: "1 0 1" green = ON, "0 X 0" corrupted red = OFF.
    // On every change the text scrolls random characters for ~0.5 s and locks in left to right.
    bool mxToggle(char const* label, bool* v) {
        ImGuiStyle& st = ImGui::GetStyle();
        float bh = ImGui::GetFrameHeight();
        ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
        ImVec2 bin = ImGui::CalcTextSize(fx::kBinOn);
        float bw = bin.x + st.FramePadding.x * 2.f;
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGuiID id = ImGui::GetID(label);
        bool pressed = ImGui::InvisibleButton(label, ImVec2(bw + (ts.x > 0.f ? st.ItemInnerSpacing.x + ts.x : 0.f), bh));
        if (pressed) *v = !*v;
        bool hov = ImGui::IsItemHovered();

        // when did this switch last change (a click or the game flipping it)? kept in ImGui's state storage
        ImGuiStorage* S = ImGui::GetStateStorage();
        ImGuiID kPrev = id ^ 0x4D58u, kAt = id ^ 0x5458u;
        double now = ImGui::GetTime();
        float cur = *v ? 1.f : 0.f, prev = S->GetFloat(kPrev, -1.f);
        if (prev < 0.f) S->SetFloat(kAt, -10.f);                         // first time seen: no animation
        else if (prev != cur) S->SetFloat(kAt, static_cast<float>(now));
        S->SetFloat(kPrev, cur);
        float t = static_cast<float>(now) - S->GetFloat(kAt, -10.f);

        char buf[16];
        char const* target = *v ? fx::kBinOn : fx::kBinOff;
        char const* shown = target;
        bool scrolling = false;
        if (t >= 0.f && t < 0.6f) {                                      // just changed: scroll, then lock in
            scrolling = !fx::scramble(target, t, static_cast<uint32_t>(now * 30.0) + id, buf, sizeof(buf));
            shown = buf;
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 a(p.x, p.y), b(p.x + bw, p.y + bh);
        ImU32 col = scrolling ? U(0.78f, 1.f, 0.82f) : *v ? U(0.f, 1.f, 0.31f) : U(1.f, 0.2f, 0.2f);
        dl->AddRectFilled(a, b, *v ? U(0.f, 0.20f, 0.05f) : U(0.24f, 0.f, 0.f));
        dl->AddRect(a, b, *v ? U(0.f, 0.95f, 0.3f, hov ? 1.f : 0.85f) : U(1.f, 0.2f, 0.2f, hov ? 1.f : 0.85f), 0.f, 0, hov ? 2.5f : 1.5f);
        ImVec2 tp(a.x + (bw - bin.x) * 0.5f, a.y + (bh - bin.y) * 0.5f);
        dl->AddText(tp, col, shown);
        if (!*v) {                                                       // OFF is "corrupted": rare one-frame white tear + torn strip
            uint32_t h = fx::hash32(id, static_cast<uint32_t>(now * 15.0));
            if (h % 40u == 0) dl->AddText(ImVec2(tp.x + 2.f, tp.y - 1.f), U(1.f, 1.f, 1.f, 0.55f), shown);
            if (h % 25u == 1) dl->AddRectFilled(ImVec2(a.x + 2.f, a.y + bh * 0.4f), ImVec2(a.x + bw * 0.7f, a.y + bh * 0.4f + 2.f), U(1.f, 0.3f, 0.3f, 0.7f));
        }
        if (ts.x > 0.f) {
            char const* end = std::strstr(label, "##");
            mxLabel(dl, ImVec2(b.x + st.ItemInnerSpacing.x, a.y + (bh - ts.y) * 0.5f), U(0.78f, 1.f, 0.82f), label, end, id);
        }
        return pressed;
    }

    // Cyber toggle: a hexagonal switch. ON = pulsing neon cyan border + inner glow, knob slid right.
    // OFF = dark grey, no glow, knob left. The label is bright cyan ON / dark grey OFF and fades in on change.
    bool cyToggle(char const* label, bool* v) {
        ImGuiStyle& st = ImGui::GetStyle();
        float bs = ImGui::GetFrameHeight(), tw = bs * 1.8f;
        ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGuiID id = ImGui::GetID(label);
        bool pressed = ImGui::InvisibleButton(label, ImVec2(tw + (ts.x > 0.f ? st.ItemInnerSpacing.x + ts.x : 0.f), bs));
        if (pressed) *v = !*v;
        bool hov = ImGui::IsItemHovered();

        ImGuiStorage* S = ImGui::GetStateStorage();
        float target = *v ? 1.f : 0.f;
        float a = S->GetFloat(id, target);
        a += (target - a) * std::min(1.f, ImGui::GetIO().DeltaTime * 12.f);
        S->SetFloat(id, a);

        float pulse = fx::pulse01(ImGui::GetTime(), 1.4f);
        float x0 = p.x, y0 = p.y, x1 = p.x + tw, y1 = p.y + bs, cy = (y0 + y1) * 0.5f, c = bs * 0.30f;
        float li = fx::laserHit(cy, g_laserY, bs * 1.2f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 hx[6] = { ImVec2(x0 + c, y0), ImVec2(x1 - c, y0), ImVec2(x1, cy), ImVec2(x1 - c, y1), ImVec2(x0 + c, y1), ImVec2(x0, cy) };
        dl->AddConvexPolyFilled(hx, 6, U(lerpf(0.07f, 0.f, a), lerpf(0.08f, 0.16f, a), lerpf(0.10f, 0.22f, a)));
        if (a > 0.02f) {
            dl->AddPolyline(hx, 6, U(0.f, 0.95f, 1.f, (0.18f + 0.22f * pulse) * a), ImDrawFlags_Closed, bs * 0.36f);   // soft outer glow
            ImVec2 in[6];
            for (int i = 0; i < 6; ++i) in[i] = ImVec2(lerpf(hx[i].x, (x0 + x1) * 0.5f, 0.28f), lerpf(hx[i].y, cy, 0.28f));
            dl->AddConvexPolyFilled(in, 6, U(0.f, 0.9f, 1.f, (0.10f + 0.12f * pulse) * a));                           // inner glow
        }
        dl->AddPolyline(hx, 6, U(lerpf(0.22f, 0.f, a), lerpf(0.25f, 0.95f, a), lerpf(0.30f, 1.f, a)), ImDrawFlags_Closed, hov ? 3.f : 2.f);
        float kx = lerpf(x0 + bs * 0.55f, x1 - bs * 0.55f, a), kr = bs * 0.26f;
        ImVec2 kn[6] = { ImVec2(kx - kr * 0.6f, cy - kr), ImVec2(kx + kr * 0.6f, cy - kr), ImVec2(kx + kr, cy),
                         ImVec2(kx + kr * 0.6f, cy + kr), ImVec2(kx - kr * 0.6f, cy + kr), ImVec2(kx - kr, cy) };
        dl->AddConvexPolyFilled(kn, 6, U(lerpf(0.25f, 0.65f, a), lerpf(0.27f, 1.f, a), lerpf(0.31f, 1.f, a)));
        if (li > 0.f) dl->AddPolyline(hx, 6, U(0.6f, 1.f, 1.f, 0.8f * li), ImDrawFlags_Closed, 2.5f);                  // laser highlight
        if (ts.x > 0.f) {
            char const* end = std::strstr(label, "##");
            ImU32 tc = U(lerpf(0.30f, 0.f, a), lerpf(0.34f, 0.94f, a), lerpf(0.40f, 1.f, a));
            dl->AddText(ImVec2(x1 + st.ItemInnerSpacing.x, cy - ts.y * 0.5f), tc, label, end);
        }
        return pressed;
    }

    // Checkbox. Cat: cat-face box (ears) that pops a paw print in. Lava: magma box with an orange check.
    bool fxCheckbox(char const* label, bool* v) {
        Kind k = curKind();
        if (k == Kind::None) return ImGui::Checkbox(label, v);
        if (k == Kind::Matrix) return mxToggle(label, v);
        if (k == Kind::Cyber)  return cyToggle(label, v);
        ImGuiStyle& st = ImGui::GetStyle();
        float bs = ImGui::GetFrameHeight();
        ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGuiID id = ImGui::GetID(label);
        bool pressed = ImGui::InvisibleButton(label, ImVec2(bs + (ts.x > 0.f ? st.ItemInnerSpacing.x + ts.x : 0.f), bs));
        if (pressed) *v = !*v;
        bool hov = ImGui::IsItemHovered();

        ImGuiStorage* S = ImGui::GetStateStorage();
        float target = *v ? 1.f : 0.f;
        float a = S->GetFloat(id, target);                       // 0..1 animation
        a += (target - a) * std::min(1.f, ImGui::GetIO().DeltaTime * 14.f);
        S->SetFloat(id, a);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float x0 = p.x, y0 = p.y, x1 = p.x + bs, y1 = p.y + bs;
        float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;

        if (k == Kind::Cat) {
            ImU32 ear = U(fx::kPeach), earIn = U(1.f, 0.86f, 0.88f);
            dl->AddTriangleFilled(ImVec2(x0 + bs * .08f, y0 + 2.f), ImVec2(x0 + bs * .12f, y0 - bs * .34f), ImVec2(x0 + bs * .42f, y0 + 2.f), ear);
            dl->AddTriangleFilled(ImVec2(x1 - bs * .08f, y0 + 2.f), ImVec2(x1 - bs * .12f, y0 - bs * .34f), ImVec2(x1 - bs * .42f, y0 + 2.f), ear);
            dl->AddTriangleFilled(ImVec2(x0 + bs * .16f, y0 + 1.f), ImVec2(x0 + bs * .17f, y0 - bs * .20f), ImVec2(x0 + bs * .34f, y0 + 1.f), earIn);
            dl->AddTriangleFilled(ImVec2(x1 - bs * .16f, y0 + 1.f), ImVec2(x1 - bs * .17f, y0 - bs * .20f), ImVec2(x1 - bs * .34f, y0 + 1.f), earIn);
            ImU32 face = U(lerpf(1.f, 1.f, a), lerpf(0.94f, 0.84f, a), lerpf(0.95f, 0.78f, a));
            float r = bs * 0.30f;
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), face, r);
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), U(1.f, hov ? 0.58f : 0.66f, hov ? 0.56f : 0.60f), r, 0, hov ? 3.f : 2.f);
            if (a > 0.02f) {
                float pop = a * (1.f + 0.35f * std::sin(a * 3.14159f));   // overshoots, then settles
                imPaw(dl, cx, cy + bs * 0.03f, bs * 0.30f * pop, 0.f, U(fx::kPeachDeep, std::min(1.f, a * 1.3f)));
                dl->AddCircleFilled(ImVec2(x0 + bs * .17f, y1 - bs * .17f), bs * .08f, U(1.f, 0.62f, 0.68f, 0.6f * a), 8);
                dl->AddCircleFilled(ImVec2(x1 - bs * .17f, y1 - bs * .17f), bs * .08f, U(1.f, 0.62f, 0.68f, 0.6f * a), 8);
            }
        } else {
            float pulse = 0.88f + 0.12f * std::sin(static_cast<float>(ImGui::GetTime()) * 3.f);
            float c = bs * 0.25f;
            ImVec2 hexo[8];
            imChamfer(x0, y0, x1, y1, c, hexo);
            dl->AddConvexPolyFilled(hexo, 8, U(0.11f, 0.03f, 0.02f));
            float m = c * 0.9f;                                            // magma core, brighter when ON
            dl->AddRectFilledMultiColor(ImVec2(x0 + m, y0 + m), ImVec2(x1 - m, y1 - m),
                U(0.22f, 0.05f, 0.02f), U(0.22f, 0.05f, 0.02f),
                U(lerpf(0.40f, 0.95f * pulse, a), lerpf(0.10f, 0.30f, a), 0.04f),
                U(lerpf(0.40f, 0.95f * pulse, a), lerpf(0.10f, 0.30f, a), 0.04f));
            dl->AddPolyline(hexo, 8, U(lerpf(0.60f, 1.f, a), lerpf(0.22f, 0.45f, a), lerpf(0.10f, 0.08f, a)),
                            ImDrawFlags_Closed, hov ? 3.f : 2.f);
            if (a > 0.02f) {                                              // orange check
                ImVec2 ck[3] = { ImVec2(x0 + bs * .24f, cy + bs * .02f), ImVec2(x0 + bs * .43f, y1 - bs * .26f),
                                 ImVec2(x1 - bs * .20f, y0 + bs * .26f) };
                dl->AddPolyline(ck, 3, U(1.f, 0.50f, 0.06f, std::min(1.f, a * 1.4f)), 0, bs * 0.16f);
                dl->AddPolyline(ck, 3, U(1.f, 0.82f, 0.35f, std::min(1.f, a * 1.4f) * 0.9f), 0, bs * 0.06f);
            }
        }
        if (ts.x > 0.f) {
            char const* end = std::strstr(label, "##");
            dl->AddText(ImVec2(x1 + st.ItemInnerSpacing.x, cy - ts.y * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), label, end);
        }
        return pressed;
    }

    void applyColors(int theme) {
        int t = std::clamp(theme, 0, cky::ThemeCount - 1);
        ImVec4 pink = themeHi(t);
        ImVec4 pinkLo = themeLo(t);
        ImVec4 pinkHi(std::min(pink.x + 0.12f, 1.f), std::min(pink.y + 0.12f, 1.f),
                      std::min(pink.z + 0.12f, 1.f), 1.f);
        auto& c = ImGui::GetStyle().Colors;
        c[ImGuiCol_WindowBg]       = ImVec4(0.07f, 0.07f, 0.10f, 0.97f);
        c[ImGuiCol_Border]         = pink;
        c[ImGuiCol_TitleBg]        = pinkLo;
        c[ImGuiCol_TitleBgActive]  = pinkLo;
        c[ImGuiCol_FrameBg]        = ImVec4(0.15f, 0.15f, 0.21f, 1.f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.30f, 1.f);
        c[ImGuiCol_FrameBgActive]  = ImVec4(0.28f, 0.28f, 0.38f, 1.f);
        c[ImGuiCol_CheckMark]      = pinkHi;
        c[ImGuiCol_SliderGrab]     = pink;
        c[ImGuiCol_SliderGrabActive] = pinkHi;
        c[ImGuiCol_Button]         = pinkLo;
        c[ImGuiCol_ButtonHovered]  = pink;
        c[ImGuiCol_ButtonActive]   = pinkHi;
        c[ImGuiCol_Header]         = pinkLo;
        c[ImGuiCol_HeaderHovered]  = pink;
        c[ImGuiCol_HeaderActive]   = pinkHi;
        c[ImGuiCol_Tab]            = ImVec4(0.14f, 0.14f, 0.19f, 1.f);
        c[ImGuiCol_TabHovered]     = pink;
        c[ImGuiCol_TabActive]      = pinkLo;

        // ---- Cat: near-white pink, peach pink accents, dark rose text ----
        fx::Kind k = fx::kindOf(t);
        auto& st = ImGui::GetStyle();
        float wr = 14.f, fr = 9.f;
        if (k == fx::Kind::Cat) {
            wr = 20.f; fr = 12.f;
            c[ImGuiCol_Text]          = ImVec4(0.55f, 0.27f, 0.34f, 1.f);
            c[ImGuiCol_TextDisabled]  = ImVec4(0.78f, 0.58f, 0.64f, 1.f);
            c[ImGuiCol_WindowBg]      = ImVec4(1.00f, 0.957f, 0.969f, 1.f);   // (hidden: drawn by fxBegin)
            c[ImGuiCol_PopupBg]       = ImVec4(1.00f, 0.95f, 0.96f, 0.98f);
            c[ImGuiCol_Border]        = ImVec4(1.00f, 0.71f, 0.63f, 1.f);
            c[ImGuiCol_TitleBg]       = ImVec4(1.00f, 0.80f, 0.74f, 1.f);
            c[ImGuiCol_TitleBgActive] = ImVec4(1.00f, 0.74f, 0.69f, 1.f);
            c[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.80f, 0.74f, 1.f);
            c[ImGuiCol_FrameBg]       = ImVec4(1.00f, 0.89f, 0.91f, 1.f);
            c[ImGuiCol_FrameBgHovered]= ImVec4(1.00f, 0.84f, 0.86f, 1.f);
            c[ImGuiCol_FrameBgActive] = ImVec4(1.00f, 0.78f, 0.80f, 1.f);
            c[ImGuiCol_CheckMark]     = ImVec4(1.00f, 0.55f, 0.58f, 1.f);
            c[ImGuiCol_SliderGrab]    = ImVec4(1.00f, 0.62f, 0.58f, 1.f);
            c[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.50f, 0.52f, 1.f);
            c[ImGuiCol_Button]        = ImVec4(1.00f, 0.80f, 0.74f, 1.f);
            c[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.72f, 0.66f, 1.f);
            c[ImGuiCol_ButtonActive]  = ImVec4(1.00f, 0.62f, 0.60f, 1.f);
            c[ImGuiCol_Header]        = ImVec4(1.00f, 0.80f, 0.74f, 1.f);
            c[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 0.72f, 0.66f, 1.f);
            c[ImGuiCol_HeaderActive]  = ImVec4(1.00f, 0.62f, 0.60f, 1.f);
            c[ImGuiCol_Tab]           = ImVec4(1.00f, 0.88f, 0.88f, 1.f);
            c[ImGuiCol_TabHovered]    = ImVec4(1.00f, 0.72f, 0.66f, 1.f);
            c[ImGuiCol_TabActive]     = ImVec4(1.00f, 0.76f, 0.70f, 1.f);
            c[ImGuiCol_Separator]     = ImVec4(1.00f, 0.78f, 0.76f, 1.f);
            c[ImGuiCol_ScrollbarBg]   = ImVec4(1.00f, 0.92f, 0.94f, 1.f);
            c[ImGuiCol_ScrollbarGrab] = ImVec4(1.00f, 0.75f, 0.72f, 1.f);
        } else if (k == fx::Kind::Lava) {
            // ---- Molten rocks: sharp corners (the chamfers are drawn by fxBegin / fxButton / fxCheckbox) ----
            wr = 0.f; fr = 0.f;
            c[ImGuiCol_Text]          = ImVec4(1.00f, 0.88f, 0.72f, 1.f);
            c[ImGuiCol_WindowBg]      = ImVec4(0.11f, 0.063f, 0.047f, 0.97f);
            c[ImGuiCol_TitleBg]       = ImVec4(0.f, 0.f, 0.f, 0.f);           // drawn by fxBegin (chamfered)
            c[ImGuiCol_TitleBgActive] = ImVec4(0.f, 0.f, 0.f, 0.f);
            c[ImGuiCol_TitleBgCollapsed] = pinkLo;
            c[ImGuiCol_FrameBg]       = ImVec4(0.20f, 0.09f, 0.06f, 1.f);
            c[ImGuiCol_FrameBgHovered]= ImVec4(0.30f, 0.12f, 0.07f, 1.f);
            c[ImGuiCol_FrameBgActive] = ImVec4(0.40f, 0.15f, 0.08f, 1.f);
            c[ImGuiCol_CheckMark]     = ImVec4(1.00f, 0.55f, 0.10f, 1.f);
            c[ImGuiCol_Tab]           = ImVec4(0.20f, 0.09f, 0.06f, 1.f);
        }
        st.FrameBorderSize = 0.f;
        if (k == fx::Kind::Matrix) {
            // ---- Matrix: pure black, terminal green, sharp corners (body / rain / buttons / toggles: fxBegin etc.) ----
            wr = 0.f; fr = 0.f;
            st.FrameBorderSize = 1.f;
            c[ImGuiCol_Text]          = ImVec4(0.78f, 1.00f, 0.82f, 1.f);
            c[ImGuiCol_TextDisabled]  = ImVec4(0.10f, 0.50f, 0.22f, 1.f);
            c[ImGuiCol_WindowBg]      = ImVec4(0.f, 0.f, 0.f, 1.f);
            c[ImGuiCol_PopupBg]       = ImVec4(0.f, 0.05f, 0.02f, 0.98f);
            c[ImGuiCol_Border]        = ImVec4(0.f, 0.90f, 0.30f, 1.f);
            c[ImGuiCol_TitleBg]       = ImVec4(0.f, 0.f, 0.f, 0.f);           // drawn by fxBegin
            c[ImGuiCol_TitleBgActive] = ImVec4(0.f, 0.f, 0.f, 0.f);
            c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.f, 0.16f, 0.06f, 1.f);
            c[ImGuiCol_FrameBg]       = ImVec4(0.f, 0.10f, 0.04f, 1.f);
            c[ImGuiCol_FrameBgHovered]= ImVec4(0.f, 0.18f, 0.07f, 1.f);
            c[ImGuiCol_FrameBgActive] = ImVec4(0.f, 0.28f, 0.10f, 1.f);
            c[ImGuiCol_CheckMark]     = ImVec4(0.f, 1.00f, 0.31f, 1.f);
            c[ImGuiCol_SliderGrab]    = ImVec4(0.f, 0.90f, 0.30f, 1.f);
            c[ImGuiCol_SliderGrabActive] = ImVec4(0.5f, 1.f, 0.6f, 1.f);
            c[ImGuiCol_Button]        = ImVec4(0.f, 0.f, 0.f, 1.f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.f, 0.16f, 0.06f, 1.f);
            c[ImGuiCol_ButtonActive]  = ImVec4(0.f, 0.28f, 0.10f, 1.f);
            c[ImGuiCol_Tab]           = ImVec4(0.f, 0.10f, 0.04f, 1.f);
            c[ImGuiCol_TabHovered]    = ImVec4(0.f, 0.40f, 0.15f, 1.f);
            c[ImGuiCol_TabActive]     = ImVec4(0.f, 0.25f, 0.10f, 1.f);
            c[ImGuiCol_Separator]     = ImVec4(0.f, 0.50f, 0.20f, 1.f);
            c[ImGuiCol_ScrollbarBg]   = ImVec4(0.f, 0.f, 0.f, 0.6f);
            c[ImGuiCol_ScrollbarGrab] = ImVec4(0.f, 0.45f, 0.18f, 1.f);
        } else if (k == fx::Kind::Cyber) {
            // ---- Cyber: deep blue-black, neon cyan (grid / laser / hex switches / chamfered buttons: fxBegin etc.) ----
            wr = 0.f; fr = 0.f;
            st.FrameBorderSize = 1.f;
            c[ImGuiCol_Text]          = ImVec4(0.78f, 0.97f, 1.00f, 1.f);
            c[ImGuiCol_TextDisabled]  = ImVec4(0.30f, 0.38f, 0.45f, 1.f);
            c[ImGuiCol_WindowBg]      = ImVec4(0.016f, 0.035f, 0.08f, 1.f);
            c[ImGuiCol_PopupBg]       = ImVec4(0.02f, 0.05f, 0.10f, 0.98f);
            c[ImGuiCol_Border]        = ImVec4(0.f, 0.80f, 0.90f, 0.8f);
            c[ImGuiCol_TitleBg]       = ImVec4(0.f, 0.f, 0.f, 0.f);           // drawn by fxBegin
            c[ImGuiCol_TitleBgActive] = ImVec4(0.f, 0.f, 0.f, 0.f);
            c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.f, 0.14f, 0.22f, 1.f);
            c[ImGuiCol_FrameBg]       = ImVec4(0.03f, 0.07f, 0.13f, 1.f);
            c[ImGuiCol_FrameBgHovered]= ImVec4(0.04f, 0.13f, 0.22f, 1.f);
            c[ImGuiCol_FrameBgActive] = ImVec4(0.05f, 0.20f, 0.32f, 1.f);
            c[ImGuiCol_CheckMark]     = ImVec4(0.f, 0.94f, 1.f, 1.f);
            c[ImGuiCol_SliderGrab]    = ImVec4(0.f, 0.85f, 0.95f, 1.f);
            c[ImGuiCol_SliderGrabActive] = ImVec4(0.6f, 1.f, 1.f, 1.f);
            c[ImGuiCol_Button]        = ImVec4(0.01f, 0.08f, 0.15f, 1.f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.f, 0.22f, 0.34f, 1.f);
            c[ImGuiCol_ButtonActive]  = ImVec4(0.f, 0.35f, 0.50f, 1.f);
            c[ImGuiCol_Tab]           = ImVec4(0.03f, 0.07f, 0.13f, 1.f);
            c[ImGuiCol_TabHovered]    = ImVec4(0.f, 0.45f, 0.60f, 1.f);
            c[ImGuiCol_TabActive]     = ImVec4(0.f, 0.28f, 0.42f, 1.f);
            c[ImGuiCol_Separator]     = ImVec4(0.f, 0.45f, 0.55f, 1.f);
            c[ImGuiCol_ScrollbarBg]   = ImVec4(0.f, 0.02f, 0.05f, 0.6f);
            c[ImGuiCol_ScrollbarGrab] = ImVec4(0.f, 0.50f, 0.60f, 1.f);
        }
        st.WindowRounding = wr;
        st.FrameRounding = st.GrabRounding = st.TabRounding = st.ChildRounding = fr;
        st.PopupRounding = fr;
    }

    void styleSetup() {
        ImGui::GetStyle() = ImGuiStyle();
        ImGui::StyleColorsDark();
        auto& s = ImGui::GetStyle();
        s.ScaleAllSizes(1.4f);
        s.FramePadding = ImVec2(14.f, 10.f);
        s.ItemSpacing = ImVec2(12.f, 12.f);
        s.GrabMinSize = 30.f;
        s.ScrollbarSize = 26.f;
        s.WindowBorderSize = 2.f;
        ImGui::GetIO().FontGlobalScale = 1.4f;
        applyColors(cky::theme);
    }

    bool minusPlus(char const* id, float& v, float delta, float lo, float hi) {
        bool changed = false;
        ImGui::PushID(id);
        ImGui::SameLine();
        if (fxButton(" - ")) { v = std::clamp(v - delta, lo, hi); changed = true; }
        ImGui::SameLine();
        if (fxButton(" + ")) { v = std::clamp(v + delta, lo, hi); changed = true; }
        ImGui::PopID();
        return changed;
    }

    void tabBot() {
        bool rec = cky::recording;
        if (fxCheckbox("Record", &rec)) {
            if (!rec) {
                cky::recording = false;
                cky::recArmed = false;
            } else {
                cky::recording = true;
                cky::playing = false;
                cky::playArmed = false;
                cky::macroTps = cky::effTps();
                cky::recArmed = (cky::step == 0);
                if (!cky::recArmed) ckyNotify("Recording starts when you restart the level");
            }
        }

        bool play = cky::playing;
        if (fxCheckbox("Playback", &play)) {
            if (!play) {
                cky::playing = false;
                cky::playArmed = false;
            } else if (cky::macro.empty()) {
                ckyNotify("No macro loaded", false);
            } else {
                cky::playing = true;
                cky::recording = false;
                cky::recArmed = false;
                cky::cursor = 0;
                cky::playArmed = (cky::step == 0);
                if (!cky::playArmed) ckyNotify("Playback ON: restart the level to start it");
            }
        }

        bool fz = cky::frozen;
        if (fxCheckbox("Frame stepper", &fz)) {
            cky::frozen = fz;
            cky::pending = 0;
            ckyApplyAudio();
        }

        bool ff = cky::frameFixes;
        if (fxCheckbox("Frame fixes", &ff)) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else cky::frameFixes = ff;
        }
        bool pf = cky::practiceFixes;
        if (fxCheckbox("Practice fixes", &pf)) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else cky::practiceFixes = pf;
        }
        {
            int fi = cky::fixInterval;
            if (ImGui::SliderInt("Fix interval (steps)", &fi, 1, 240)) {
                if (cky::recording) ckyNotify("Stop recording first", false);
                else cky::fixInterval = std::clamp(fi, 1, 240);
            }
            fxCheckbox("Fix velocity", &cky::fixVelocity);
        }

        ImGui::Separator();
        ImGui::SliderInt("Macro slot", &cky::slot, 1, cky::MAX_SLOTS);

        if (fxButton("Save macro")) {
            bool ok = cky::save();
            ckyNotify(ok ? fmt::format("Saved to slot {}", cky::slot).c_str() : "Save failed", ok);
        }
        ImGui::SameLine();
        if (fxButton("Load macro")) {
            bool ok = cky::load();
            ckyNotify(ok ? fmt::format("Loaded slot {}", cky::slot).c_str() : "Load failed", ok);
        }
        if (fxButton("Clear macro")) {
            cky::clearMacro();
            ckyNotify("Macro buffer cleared");
        }
        ImGui::SameLine();
        if (fxButton("Import GDR file")) ckyImportGdr();

        ImGui::SliderInt("Input offset", &cky::frameOffset, -10, 10);

        ImGui::Separator();
        ImGui::Text("Inputs: %llu", static_cast<unsigned long long>(cky::macro.size()));
        ImGui::Text("Macro TPS: %d", static_cast<int>(cky::macroTps));
        ImGui::Text("Slot %d: %s", cky::slot, cky::slotExists() ? "saved" : "empty");
    }

    void tabGlobal() {
        bool tpsOn = cky::tpsOn;
        if (fxCheckbox("TPS bypass", &tpsOn)) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else { cky::tpsOn = tpsOn; cky::resetMeasure(); }
        }

        float t = cky::tps;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderFloat("##tps", &t, cky::TPS_MIN, cky::TPS_MAX, "TPS %.0f")) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else { cky::tps = std::round(t); cky::tpsOn = true; cky::resetMeasure(); }
        }
        float t2 = cky::tps;
        if (minusPlus("tps", t2, cky::tpsStep, cky::TPS_MIN, cky::TPS_MAX)) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else { cky::tps = t2; cky::tpsOn = true; cky::resetMeasure(); }
        }

        if (fxButton(fmt::format("Step size: {}", static_cast<int>(cky::tpsStep)).c_str())) {
            float n = cky::nextVal(cky::tpsStepList, cky::tpsStep, 1);
            if (n == cky::tpsStep) n = cky::tpsStepList.front();
            cky::tpsStep = n;
        }
        ImGui::SameLine();
        std::string hi = (cky::tpsOn && cky::tps >= 480.f)
            ? fmt::format("High TPS: {}", static_cast<int>(cky::tps))
            : std::string("High TPS: OFF");
        if (fxButton(hi.c_str())) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else {
                cky::tps = cky::nextHighTps(cky::tpsOn ? cky::tps : 240.f);
                cky::tpsOn = true;
                cky::resetMeasure();
            }
        }
        if (cky::actualTps > 0.f) {
            ImGui::TextDisabled("Real TPS: %d", static_cast<int>(cky::actualTps + 0.5f));
        }

        ImGui::Separator();
        bool spOn = cky::speedOn;
        if (fxCheckbox("Speedhack", &spOn)) {
            cky::speedOn = spOn;
            ckyApplyAudio();
        }
        float sp = cky::speed;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderFloat("##speed", &sp, 0.01f, 4.f, "%.2fx")) {
            cky::speed = std::clamp(std::round(sp * 100.f) / 100.f, 0.01f, 4.f);
            cky::speedOn = true;
            ckyApplyAudio();
        }
        float sp2 = cky::speed;
        if (minusPlus("speed", sp2, 0.01f, 0.01f, 4.f)) {
            cky::speed = std::round(sp2 * 100.f) / 100.f;
            cky::speedOn = true;
            ckyApplyAudio();
        }
        if (fxButton("Slower")) {
            cky::speed = cky::nextVal(cky::speedList, cky::speed, -1);
            cky::speedOn = true;
            ckyApplyAudio();
        }
        ImGui::SameLine();
        if (fxButton("Faster")) {
            cky::speed = cky::nextVal(cky::speedList, cky::speed, 1);
            cky::speedOn = true;
            ckyApplyAudio();
        }

        bool au = cky::audioSpeed;
        if (fxCheckbox("Audio follows speed", &au)) {
            cky::audioSpeed = au;
            ckyApplyAudio();
        }
}
    void tabPlayer() {
        bool nc = cky::noclip;
        if (fxCheckbox("Noclip", &nc)) {
            cky::noclip = nc;
            cky::ncReset();
        }
        fxCheckbox("Safe mode", &cky::safeMode);
    }

    void tabLabels() {
        bool ch = false;
        ch |= fxCheckbox("CPS", &cky::clickHudOn);
        ch |= fxCheckbox("FPS", &cky::lblFps);
        ch |= fxCheckbox("Noclip accuracy", &cky::ncShowAcc);
        ch |= fxCheckbox("Noclip deaths", &cky::ncShowDeaths);
        ch |= fxCheckbox("Startpos run", &cky::lblRun);
        ch |= fxCheckbox("Level time", &cky::lblTime);
        ch |= fxCheckbox("Best run", &cky::lblBest);
        if (ch) cky::labelsSave(); // remembered between launches
    }

    void tabDisplay() {
        bool hb = cky::hitboxes;
        if (fxCheckbox("Hitboxes", &hb)) {
            cky::hitboxes = hb;
            ckyApplyHitboxes();
        }
        bool ht = cky::hitboxTrail;
        if (fxCheckbox("Hitbox trail", &ht)) {
            cky::hitboxTrail = ht;
            ckyTrailApply();
        }
        if (ImGui::TreeNode("Hitbox trail settings")) {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
            if (ImGui::SliderInt("Trail length (steps)", &cky::trailLen, 20, 1200)) ckyTrailSave();
            if (ImGui::ColorEdit4("Outer hitbox", cky::trailOuter)) ckyTrailSave();
            if (ImGui::ColorEdit4("Inner hitbox", cky::trailInner)) ckyTrailSave();
            ImGui::TreePop();
        }
        bool hud = cky::hudOn;
        if (fxCheckbox("HUD", &hud)) {
            cky::hudOn = hud;
            if (auto h = ckyHudNode()) h->setVisible(cky::hudOn);
        }
        float hs = cky::hudScale;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderFloat("HUD size", &hs, 0.5f, 2.f, "%.1fx")) {
            cky::hudScale = hs;
            ckyApplyHudScale();
        }
        if (ImGui::TreeNode("HUD settings")) {
            fxCheckbox("Show frame stepper", &cky::hudStepShow);
            fxCheckbox("Show speedhack", &cky::hudSpeedShow);
            bool sb = cky::stepperBtns;
            if (fxCheckbox("Frame stepper buttons", &sb)) {
                cky::stepperBtns = sb;
                if (!sb) {
                    cky::frozen = false;
                    cky::pending = 0;
                    ckyApplyAudio();
                }
            }
            bool sh = cky::speedBtns;
            if (fxCheckbox("Speedhack buttons", &sh)) {
                cky::speedBtns = sh;
                ckyApplyAudio();
            }
            bool bs = cky::backStepOn;
            if (fxCheckbox("Back step", &bs)) {
                cky::backStepOn = bs;
                if (!bs) ckyBackStepClear();
            }
            fxCheckbox("Buffer while playing", &cky::backAlways);
            ckyHudRefresh();
            ImGui::TreePop();
        }
        bool sp = cky::startPosOn;
        if (fxCheckbox("StartPos switcher", &sp)) {
            cky::startPosOn = sp;
            ckySetStartPosVisible(cky::startPosOn);
        }
        fxCheckbox("Input overlay", &cky::overlayOn);

        bool tj = cky::trajectory;
        if (fxCheckbox("Show trajectory", &tj)) {
            cky::trajectory = tj;
            if (!tj) ckyTrajClear();
            else cky::dirty = true;
        }
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        ImGui::SliderInt("Trajectory length", &cky::trajLen, 60, 480);
    }

    void tabClicks() {
        bool cb = cky::clickbotOn;
        if (fxCheckbox("Clickbot sounds", &cb)) {
            cky::clickbotOn = cb;
            ckyClickRescan();
        }
        if (fxButton(" < ")) ckyClickPackStep(-1);
        ImGui::SameLine();
        ImGui::Text("Pack: %s", ckyClickPackName().c_str());
        ImGui::SameLine();
        if (fxButton(" > ")) ckyClickPackStep(1);

        int vol = cky::clickVol;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderInt("Volume", &vol, 0, 200, "%d%%")) {
            cky::clickVol = vol;
            Mod::get()->setSavedValue<int>("click-vol", cky::clickVol);
        }
        ImGui::TextDisabled("Packs go in game/clicks/<PackName>/");
        ImGui::TextDisabled("with player1/clicks, releases, softclicks...");
    }

    void tabSettings() {
        float is = cky::iconScale;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderFloat("Cky icon size", &is, 0.5f, 2.f, "%.1fx")) {
            cky::iconScale = is;
            if (auto scene = CCDirector::get()->getRunningScene()) {
                if (auto ic = scene->getChildByIDRecursive("cky-icon")) ic->setScale(cky::iconScale);
            }
        }
        if (fxButton("Reset icon position")) {
            cky::savePos("cky-icon", CCPoint(-99999.f, -99999.f));
            if (auto scene = CCDirector::get()->getRunningScene()) {
                if (auto ic = scene->getChildByIDRecursive("cky-icon")) ic->setPosition(ckyDefaultIconPos());
            }
        }
        fxCheckbox("Checkpoint telemetry", &cky::ckptTelemetry);

        int th = std::clamp(cky::theme, 0, cky::ThemeCount - 1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderInt("Theme", &th, 0, cky::ThemeCount - 1, cky::kThemeDefs[th].name)) {
            cky::theme = th;
            Mod::get()->setSavedValue<int>("theme", cky::theme);
            applyColors(th);
        }

        ImGui::Separator();
        ImGui::Text("ChxrCky Macro Bot v1.0.5");
        ImGui::TextDisabled("(c) 2026 ChxrCky - GNU GPL v3.0");
        ImGui::TextDisabled("Free software, NO WARRANTY.");
        if (fxButton("Open GPL-3.0 license")) {
            web::openLinkInBrowser("https://www.gnu.org/licenses/gpl-3.0.html");
        }
    }

    void drawMenu() {
        if (!cky::imguiOpen) return;

        ImVec2 disp = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(
            ImVec2(std::min(disp.x * 0.85f, 1100.f), std::min(disp.y * 0.85f, 700.f)),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(disp.x * 0.5f, disp.y * 0.5f),
            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

        bool open = true;
        // Cat / Lava draw their own body (rounded pink / chamfered metal) behind the widgets
        ImGuiWindowFlags flags = curKind() != Kind::None ? ImGuiWindowFlags_NoBackground : 0;
        if (ImGui::Begin("ChxrCky Macro Bot", &open, flags)) {
            ImVec2 fxPos = ImGui::GetWindowPos(), fxSize = ImGui::GetWindowSize();
            fxBegin(fxPos, fxSize);
            if (ImGui::BeginTabBar("cky-tabs")) {
                if (ImGui::BeginTabItem("Bot"))      { tabBot();      ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Global"))   { tabGlobal();   ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Player"))   { tabPlayer();   ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Display"))  { tabDisplay();  ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Labels"))   { tabLabels();   ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Clicks"))   { tabClicks();   ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Settings")) { tabSettings(); ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
            fxEnd(fxPos, fxSize);
        }
        ImGui::End();
        ckyHudRefresh();
        if (!open) cky::imguiOpen = false;
    }
}

$on_mod(Loaded) {
    if (!wantsImGui()) return;
    cky::theme = Mod::get()->getSavedValue<int>("theme", 0);
    ImGuiCocos::get().setup(styleSetup).draw(drawMenu);
    g_ready = true;
}

bool ckyUiIsImGui() { return g_ready; }
void ckyImGuiToggle() { cky::imguiOpen = !cky::imguiOpen; }

#else

bool ckyUiIsImGui() { return false; }
void ckyImGuiToggle() {}

#endif

// END OF FILE (imgui_ui.cpp)
