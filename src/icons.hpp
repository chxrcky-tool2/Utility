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

// Round button: neon ring + dark face + glowing Cky logo
inline CCNode* ckyMakeLogo(float d) {
    auto container = CCNode::create();
    container->setContentSize({d, d});
    container->setAnchorPoint({0.5f, 0.5f});

    float R = d / 2.f;
    auto dn = CCDrawNode::create();
    dn->drawDot({R, R}, R, ccc4f(0.35f, 0.85f, 1.f, 1.f));
    dn->drawDot({R, R}, R - 1.4f, ccc4f(0.04f, 0.05f, 0.17f, 1.f));
    dn->drawDot({R, R}, R - 3.4f, ccc4f(0.06f, 0.08f, 0.25f, 1.f));

    float cell = (d * 0.72f) / 64.f;
    float ox = R - 32.f * cell;
    float oy = R - 24.5f * cell;
    size_t n = sizeof(CKY_LOGO) / 2;
    float k = d / 44.f;

    struct Layer { float r; ccColor4F c; };
    Layer layers[3] = {
        { 1.7f,  ccc4f(0.15f * 0.12f, 0.75f * 0.12f, 1.f * 0.12f, 0.12f) },
        { 1.1f,  ccc4f(0.25f * 0.30f, 0.82f * 0.30f, 1.f * 0.30f, 0.30f) },
        { 0.62f, ccc4f(0.85f, 1.f, 1.f, 1.f) }
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
