#pragma once
#include "state.hpp"

// Neon "Cky" logo (x,y pairs on a 64x49 grid)
inline const uint8_t CKY_LOGO[] = {
    1,28,1,27,1,26,1,25,1,24,1,23,1,22,1,21,1,20,1,19,2,31,2,30,
    2,29,2,28,2,19,2,18,2,17,3,33,3,32,3,31,3,17,3,16,4,35,4,34,
    4,33,4,16,5,37,5,36,5,35,5,16,5,15,6,39,6,38,6,37,6,15,7,40,
    7,39,7,15,8,41,8,40,8,16,8,15,9,43,9,42,9,41,9,16,10,44,10,43,
    10,16,11,45,11,44,11,17,11,16,12,45,12,17,13,46,13,45,13,18,13,17,14,47,
    14,46,14,18,15,47,15,19,15,18,16,47,16,19,17,47,17,40,17,39,17,38,17,20,
    18,47,18,46,18,42,18,41,18,40,18,21,18,20,19,46,19,45,19,44,19,43,19,42,
    19,22,19,21,20,23,20,22,21,24,21,23,22,24,22,17,23,25,23,24,23,21,23,20,
    23,19,23,18,23,17,24,25,24,24,24,23,24,22,24,21,25,28,25,27,25,26,25,25,
    26,33,26,32,26,31,26,30,26,29,26,28,26,27,26,26,26,25,27,36,27,35,27,34,
    27,33,27,28,27,27,27,25,27,24,28,39,28,38,28,37,28,36,28,28,28,24,29,42,
    29,41,29,40,29,39,29,29,29,28,29,24,29,23,30,45,30,44,30,43,30,42,30,29,
    30,23,31,30,31,23,32,31,32,30,32,23,32,22,33,32,33,31,33,22,34,32,34,22,
    35,33,35,32,35,23,35,22,36,23,37,23,38,24,38,6,38,5,38,4,38,3,39,25,
    39,24,39,8,39,7,39,6,39,2,40,26,40,25,40,10,40,9,40,8,40,2,41,26,
    41,11,41,10,41,2,42,28,42,27,42,26,42,25,42,24,42,23,42,13,42,12,42,11,
    42,2,43,31,43,30,43,29,43,28,43,22,43,14,43,13,43,3,43,2,44,32,44,31,
    44,22,44,15,44,14,44,4,44,3,45,23,45,22,45,16,45,15,45,5,45,4,46,23,
    46,17,46,16,46,7,46,6,46,5,47,24,47,23,47,18,47,17,47,9,47,8,47,7,
    48,26,48,25,48,24,48,18,48,11,48,10,48,9,49,27,49,26,49,19,49,18,49,14,
    49,13,49,12,49,11,50,28,50,27,50,19,50,18,50,17,50,16,50,15,50,14,51,28,
    51,21,51,20,51,19,52,31,52,30,52,29,52,28,52,27,52,26,52,25,52,24,52,23,
    52,22,52,21,53,31,53,22,54,23,54,22,55,23,56,24,56,23,57,25,57,24,58,25,
    59,26,59,25,60,26,61,27,62,28,62,27,63,28,63,27
};

// Round button: neon ring + dark face + glowing Cky logo.
// The disc is built from 64-sided polygons (a true circle whatever the dot shader does) and the ring / face
// pieces do NOT overlap, so `alpha` can be applied to every colour (premultiplied, as CCDrawNode wants)
// and the idle 0.5 version still looks like the full one, just see-through.
inline CCNode* ckyMakeLogo(float d, float alpha = 1.f) {
    auto container = CCNode::create();
    container->setContentSize({d, d});
    container->setAnchorPoint({0.5f, 0.5f});

    float R = d / 2.f;
    auto dn = CCDrawNode::create();
    auto pm = [&](float r, float g, float b, float a = 1.f) {
        a *= alpha;
        return ccc4f(r * a, g * a, b * a, a);
    };
    constexpr int SEG = 64;
    constexpr float TAU = 6.28318530718f;
    auto pt = [&](float rad, int i) {
        float a = TAU * static_cast<float>(i) / SEG;
        return CCPoint(R + std::cos(a) * rad, R + std::sin(a) * rad);
    };
    auto disc = [&](float rad, ccColor4F c) {
        CCPoint v[SEG];
        for (int i = 0; i < SEG; ++i) v[i] = pt(rad, i);
        dn->drawPolygon(v, SEG, c, 0.f, c);
    };
    auto band = [&](float r0, float r1, ccColor4F c) {   // ring between two radii, as 64 touching quads
        for (int i = 0; i < SEG; ++i) {
            CCPoint q[4] = { pt(r1, i), pt(r1, i + 1), pt(r0, i + 1), pt(r0, i) };
            dn->drawPolygon(q, 4, c, 0.f, c);
        }
    };
    band(R - 1.4f, R,       pm(0.35f, 0.85f, 1.f));        // neon ring
    band(R - 3.4f, R - 1.4f, pm(0.04f, 0.05f, 0.17f));      // dark inner rim
    disc(R - 3.4f,           pm(0.06f, 0.08f, 0.25f));      // face

    float cell = (d * 0.72f) / 64.f;
    float ox = R - 32.f * cell;
    float oy = R - 24.5f * cell;
    size_t n = sizeof(CKY_LOGO) / 2;
    float k = d / 44.f;

    struct Layer { float r; ccColor4F c; };
    Layer layers[3] = {
        { 1.7f,  pm(0.15f, 0.75f, 1.f, 0.12f) },
        { 1.1f,  pm(0.25f, 0.82f, 1.f, 0.30f) },
        { 0.62f, pm(0.85f, 1.f, 1.f, 1.f) }
    };
    for (auto const& l : layers) {
        for (size_t i = 0; i < n; ++i) {
            float x = ox + static_cast<float>(CKY_LOGO[i * 2]) * cell;
            float y = oy + static_cast<float>(CKY_LOGO[i * 2 + 1]) * cell;
            dn->drawDot({x, y}, l.r * k, l.c);
        }
    }
    container->addChild(dn);
    return container;
}

// ---- shared helpers for the themed round buttons below ----
// Same trick as ckyMakeLogo: every shape is a polygon built from 64-sided circles, and the big fills do NOT
// overlap, so `alpha` is applied to each colour (premultiplied, as CCDrawNode wants) and the idle 0.5 copy still
// looks like the full one, just see-through.
namespace cky_icons {
    constexpr int   SEG = 64;
    constexpr float TAU = 6.28318530718f;

    inline ccColor4F pm(float r, float g, float b, float a, float alpha) {
        a *= alpha;
        return ccc4f(r * a, g * a, b * a, a);
    }
    inline CCPoint circlePt(float R, float rad, int i) {
        float a = TAU * static_cast<float>(i) / SEG;
        return CCPoint(R + std::cos(a) * rad, R + std::sin(a) * rad);
    }
    inline void disc(CCDrawNode* dn, float R, float rad, ccColor4F c) {
        CCPoint v[SEG];
        for (int i = 0; i < SEG; ++i) v[i] = circlePt(R, rad, i);
        dn->drawPolygon(v, SEG, c, 0.f, c);
    }
    inline void band(CCDrawNode* dn, float R, float r0, float r1, ccColor4F c) {
        for (int i = 0; i < SEG; ++i) {
            CCPoint q[4] = { circlePt(R, r1, i), circlePt(R, r1, i + 1), circlePt(R, r0, i + 1), circlePt(R, r0, i) };
            dn->drawPolygon(q, 4, c, 0.f, c);
        }
    }
}

// Cat theme button: a round peach-pink cat face with two ears, whiskers and the word "Cat" inside
// (the other themes show "Cky"). Same size / hit area as the Cky logo.
inline CCNode* ckyMakeCatLogo(float d, float alpha = 1.f) {
    using namespace cky_icons;
    auto container = CCNode::create();
    container->setContentSize({d, d});
    container->setAnchorPoint({0.5f, 0.5f});

    float R = d / 2.f;
    auto dn = CCDrawNode::create();
    auto P = [&](float r, float g, float b, float a = 1.f) { return pm(r, g, b, a, alpha); };
    auto ang = [&](float deg, float rad) {          // point at an angle on a circle around the centre
        float a = deg * 3.14159265f / 180.f;
        return CCPoint(R + std::cos(a) * rad, R + std::sin(a) * rad);
    };

    // ears: two triangles each (tip -> two base points on the outer ring, split at the middle of the arc), so the
    // ears never overlap the ring and the see-through copy has no darker spots
    ccColor4F ear = P(1.f, 0.62f, 0.60f);
    for (float side : { 1.f, -1.f }) {
        float mid = side > 0.f ? 55.f : 125.f;      // right ear / left ear, measured from the +x axis
        CCPoint a = ang(mid - 17.f, R), m = ang(mid, R), b = ang(mid + 17.f, R);
        CCPoint tip = ang(mid + side * -2.f, R * 1.42f);
        CCPoint t1[3] = { a, tip, m };
        CCPoint t2[3] = { b, tip, m };
        dn->drawPolygon(t1, 3, ear, 0.f, ear);
        dn->drawPolygon(t2, 3, ear, 0.f, ear);
    }

    band(dn, R, R - 2.2f, R, P(1.f, 0.62f, 0.60f));       // peach ring
    disc(dn, R, R - 2.2f, P(1.f, 0.94f, 0.95f));          // near-white pink face

    // whiskers (3 per side) and blush
    ccColor4F wh = P(0.86f, 0.52f, 0.60f, 0.9f);
    for (float side : { 1.f, -1.f }) {
        float x0 = R + side * R * 0.58f, x1 = R + side * R * 0.96f;
        dn->drawSegment({ x0, R - R * 0.10f }, { x1, R - R * 0.02f }, 0.45f, wh);
        dn->drawSegment({ x0, R - R * 0.20f }, { x1, R - R * 0.22f }, 0.45f, wh);
        dn->drawSegment({ x0, R - R * 0.30f }, { x1, R - R * 0.42f }, 0.45f, wh);
    }
    container->addChild(dn);

    // "Cat" (dark rose on the light face = high contrast), scaled to fit inside the face
    auto lbl = CCLabelBMFont::create("Cat", "bigFont.fnt");
    lbl->setScale(1.f);
    float w = std::max(1.f, lbl->getContentSize().width);
    lbl->setScale(std::min(0.5f, d * 0.60f / w));
    lbl->setColor(ccc3(120, 36, 64));
    lbl->setOpacity(static_cast<GLubyte>(std::clamp(alpha, 0.f, 1.f) * 255.f));
    lbl->setPosition({ R, R + R * 0.10f });
    container->addChild(lbl, 1);
    return container;
}

// Molten rocks theme button: a magma icon. Obsidian disc, glowing cracks that fan out from a molten core, hot
// heated-metal ring. No text. Same size / hit area as the Cky logo.
inline CCNode* ckyMakeMagmaLogo(float d, float alpha = 1.f) {
    using namespace cky_icons;
    auto container = CCNode::create();
    container->setContentSize({d, d});
    container->setAnchorPoint({0.5f, 0.5f});

    float R = d / 2.f;
    auto dn = CCDrawNode::create();
    auto P = [&](float r, float g, float b, float a = 1.f) { return pm(r, g, b, a, alpha); };

    band(dn, R, R - 1.8f, R, P(1.f, 0.50f, 0.10f));        // glowing hot rim
    band(dn, R, R - 3.4f, R - 1.8f, P(0.05f, 0.03f, 0.03f)); // obsidian inner rim
    disc(dn, R, R - 3.4f, P(0.12f, 0.06f, 0.05f));          // basalt face

    // cracks in unit coordinates (centre = 0,0, all inside 0.9): a soft glow underneath, a hot core on top
    struct Seg { float x0, y0, x1, y1; };
    static constexpr Seg cracks[] = {
        { 0.00f,  0.00f,  0.25f,  0.30f }, { 0.25f,  0.30f,  0.20f,  0.62f }, { 0.20f,  0.62f,  0.45f,  0.82f },
        { 0.00f,  0.00f, -0.30f,  0.18f }, {-0.30f,  0.18f, -0.62f,  0.22f }, {-0.62f,  0.22f, -0.84f,  0.45f },
        { 0.00f,  0.00f, -0.12f, -0.38f }, {-0.12f, -0.38f, -0.40f, -0.58f }, {-0.40f, -0.58f, -0.46f, -0.82f },
        { 0.00f,  0.00f,  0.40f, -0.20f }, { 0.40f, -0.20f,  0.70f, -0.28f }, { 0.70f, -0.28f,  0.86f, -0.55f },
        { 0.25f,  0.30f,  0.55f,  0.34f }, {-0.62f,  0.22f, -0.70f, -0.05f }, {-0.12f, -0.38f,  0.12f, -0.66f },
    };
    float s = R - 5.f;
    for (auto const& c : cracks)
        dn->drawSegment({ R + c.x0 * s, R + c.y0 * s }, { R + c.x1 * s, R + c.y1 * s }, 1.9f, P(1.f, 0.38f, 0.06f, 0.30f));
    for (auto const& c : cracks)
        dn->drawSegment({ R + c.x0 * s, R + c.y0 * s }, { R + c.x1 * s, R + c.y1 * s }, 0.75f, P(1.f, 0.82f, 0.30f, 1.f));
    dn->drawDot({ R, R }, s * 0.30f, P(1.f, 0.42f, 0.06f, 0.55f));     // molten core
    dn->drawDot({ R, R }, s * 0.17f, P(1.f, 0.90f, 0.45f, 1.f));
    container->addChild(dn);
    return container;
}

// Clock icon (speedhack). Child tag 91 = ON (cyan), tag 92 = OFF (grey)
inline CCNode* ckyClockIcon(float s) {
    auto n = CCNode::create();
    n->setContentSize({s, s});
    n->setAnchorPoint({0.5f, 0.5f});
    float c = s / 2.f;
    auto build = [&](ccColor4F col, int t) {
        auto d = CCDrawNode::create();
        d->drawDot({c, c}, c, col);
        d->drawDot({c, c}, c - s * 0.11f, ccc4f(0.04f, 0.07f, 0.2f, 1.f));
        d->drawSegment({c, c}, {c, c + c * 0.55f}, s * 0.05f, col);
        d->drawSegment({c, c}, {c + c * 0.42f, c}, s * 0.05f, col);
        d->drawDot({c, c}, s * 0.07f, col);
        d->setTag(t);
        n->addChild(d);
        return d;
    };
    build(ccc4f(0.35f, 0.85f, 1.f, 1.f), 91);
    build(ccc4f(0.45f, 0.45f, 0.5f, 1.f), 92)->setVisible(false);
    return n;
}

// Blue right arrow (frame stepper)
inline CCNode* ckyArrowIcon(float s) {
    auto n = CCNode::create();
    n->setContentSize({s, s});
    n->setAnchorPoint({0.5f, 0.5f});
    float c = s / 2.f;
    ccColor4F blue = ccc4f(0.2f, 0.55f, 1.f, 1.f);
    auto d = CCDrawNode::create();
    float th = s * 0.1f;
    d->drawSegment({c - s * 0.2f, c + s * 0.36f}, {c + s * 0.22f, c}, th, blue);
    d->drawSegment({c + s * 0.22f, c}, {c - s * 0.2f, c - s * 0.36f}, th, blue);
    d->drawDot({c - s * 0.2f, c + s * 0.36f}, th, blue);
    d->drawDot({c - s * 0.2f, c - s * 0.36f}, th, blue);
    d->drawDot({c + s * 0.22f, c}, th, blue);
    n->addChild(d);
    return n;
}

// END OF FILE (icons.hpp)
