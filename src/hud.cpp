// hud.cpp — in-level HUD: frame stepper (top-left), speedhack (top-right), big accuracy
//
// Per-theme look (all colours still come from themes.hpp):
//   Cyber        deep-blue glass, thin chamfered cyan outline, tight padding
//   Matrix       black terminal box, thin green border, drop-shadowed text, compact layout
//   Cat          pink-white glass, hairline peach border, tight spacing
//   Molten rocks charcoal panel, glowing cracked-lava texture, cracked-obsidian border, rising embers
//   every other theme: unchanged
// The panel fill of EVERY theme follows cky::hudOpacity (the "HUD opacity" setting). Only this in-level HUD does:
// the Cocos panel and the ImGui menu are always fully opaque.
#include "state.hpp"
#include "icons.hpp"
#include "fx.hpp"

namespace {
    namespace fx = cky::fx;
    using fx::Kind;

    // label tags
    constexpr int HudStepLabel = 10, HudSpeedLabel = 11, HudNcLabel = 12;
    constexpr int HudStepInfo = 15, HudSpeedInfo = 16;
    constexpr int ShadowOff = 100;                // a label's drop shadow is the label at tag + 100
    // button tags
    constexpr int HudFreeze = 1, HudStep = 2, HudSpeedDown = 3, HudSpeedUp = 4, HudClock = 5, HudBack = 6;
    // glyph / pill tags
    constexpr int TagPillA = 81, TagPillB = 82;   // pill: normal / alternate look
    constexpr int TagGlyphA = 61, TagGlyphB = 62; // freeze button: pause / play

    ccColor4F const kGrey   = { 0.45f, 0.45f, 0.52f, 1.f };
    ccColor4F const kFrozen = { 1.00f, 0.82f, 0.30f, 1.f };

    // same colours as the ImGui / Cocos menu themes (all defined in themes.hpp)
    int themeIdx() { return std::clamp(cky::theme, 0, cky::ThemeCount - 1); }
    Kind kind() { return fx::kindOf(themeIdx()); }

    // the "HUD opacity" setting: fill opacity of every HUD panel. Outlines stay crisper than the fill so a very
    // faint panel still reads as a panel.
    float hudA()  { return std::clamp(cky::hudOpacity, 0.1f, 1.f); }
    float edgeA() { return 0.55f + 0.45f * hudA(); }

    // ---------- layout: every number that depends on the theme lives here ----------
    struct Lay {
        float pillH, stepW, speedW, btnD, margin, radius, border;
        float stepPad, speedPad, gap, lblGap;       // padding inside the pill, button pitch, gap button -> text
        float lblStep, lblSpeed, lblInfo;           // label scales
        float rowY, infoY, infoX;                   // main row / info line / info inset
        bool  panels;                               // (every theme has a panel now; kept for the layout table)
    };
    //                                 pillH stepW speedW btnD  mrg  rad  bord  sPad  vPad  gap  lblG  lS    lV    lI    rowY    infoY  infoX  panels
    constexpr Lay kLayDefault = { 44.f, 190.f, 158.f, 28.f, 10.f, 12.f, 1.6f, 6.f, 8.f, 32.f, 8.f, .34f, .42f, .20f, -17.f, -37.f, 10.f, true  };
    constexpr Lay kLayCyber   = { 34.f, 168.f, 138.f, 22.f,  6.f,  5.f, 0.7f, 5.f, 6.f, 26.f, 6.f, .30f, .36f, .17f, -13.f, -27.5f, 7.f, true  };
    constexpr Lay kLayMatrix  = { 30.f, 150.f, 122.f, 20.f,  6.f,  0.f, 0.7f, 5.f, 5.f, 23.f, 5.f, .28f, .32f, .16f, -11.5f, -24.f, 5.f, true  };
    constexpr Lay kLayCat     = { 36.f, 172.f, 142.f, 23.f,  8.f, 10.f, 0.7f, 5.f, 6.f, 27.f, 6.f, .31f, .37f, .17f, -13.5f, -28.5f, 8.f, true  };

    Lay const& L() {
        switch (kind()) {
            case Kind::Cyber:  return kLayCyber;
            case Kind::Matrix: return kLayMatrix;
            case Kind::Cat:    return kLayCat;
            default:           return kLayDefault;
        }
    }

    float stepBtnX(int i) { auto const& l = L(); return l.stepPad + l.btnD * 0.5f + l.gap * static_cast<float>(i); }
    float stepLabelX(bool btns, bool back) {
        auto const& l = L();
        if (!btns) return l.stepPad + l.lblGap;
        return stepBtnX(back ? 2 : 1) + l.btnD * 0.5f + l.lblGap;
    }
    float speedClockX()  { auto const& l = L(); return -l.speedW + l.speedPad + l.btnD * 0.5f; }
    float speedMinusX()  { return speedClockX() + L().gap; }
    float speedPlusX()   { auto const& l = L(); return -l.speedPad - l.btnD * 0.5f; }
    float speedLabelX(bool btns) { return btns ? (speedMinusX() + speedPlusX()) * 0.5f : -L().speedW * 0.5f; }

    // ---------- colours ----------
    ccColor4F themeHi() {
        auto const& v = cky::kThemeDefs[themeIdx()].hi;
        return { v[0], v[1], v[2], 1.f };
    }
    ccColor4F themeLo() {
        auto const& v = cky::kThemeDefs[themeIdx()].lo;
        return { v[0], v[1], v[2], 1.f };
    }
    // dark panel tinted with the theme colour (themes that have no look of their own)
    ccColor4F themePanel() {
        auto lo = themeLo();
        float a = hudA();
        return ccc4f((0.04f + lo.r * 0.22f) * a, (0.04f + lo.g * 0.22f) * a, (0.07f + lo.b * 0.22f) * a, a);   // premultiplied
    }
    ccColor4F themeBtnBg() {
        auto lo = themeLo();
        return ccc4f(lo.r * 0.75f, lo.g * 0.75f, lo.b * 0.75f, 1.f);
    }
    ccColor3B toC3(ccColor4F c) {
        return ccc3(static_cast<GLubyte>(std::min(1.f, c.r) * 255.f),
                    static_cast<GLubyte>(std::min(1.f, c.g) * 255.f),
                    static_cast<GLubyte>(std::min(1.f, c.b) * 255.f));
    }

    // text colours: Cat's glass is light, so its text is dark rose; everything else is light on dark
    bool lightGlass() { return kind() == Kind::Cat; }
    ccColor3B textMain() { return lightGlass() ? ccc3(128, 50, 74)   : ccc3(255, 255, 255); }
    ccColor3B textDim()  { return lightGlass() ? ccc3(150, 96, 112)  : ccc3(172, 174, 190); }
    ccColor3B textWarn() { return lightGlass() ? ccc3(196, 104, 24)  : ccc3(255, 205, 120); }
    ccColor3B textOk()   { return lightGlass() ? ccc3(36, 140, 76)   : ccc3(120, 255, 150); }
    ccColor3B textAlert(){ return lightGlass() ? ccc3(204, 112, 16)  : ccc3(255, 190, 90); }
    ccColor3B textInfo() {
        if (lightGlass()) return ccc3(150, 52, 84);
        if (kind() == Kind::Lava) return ccc3(255, 228, 90);      // bright yellow: readable on the lava panel
        ccColor4F hi = themeHi();
        return toC3(ccc4f(0.5f + hi.r * 0.5f, 0.5f + hi.g * 0.5f, 0.5f + hi.b * 0.5f, 1.f));
    }
    ccColor4F glyphCol() {
        switch (kind()) {
            case Kind::Cat:    return ccc4f(0.55f, 0.24f, 0.33f, 1.f);
            case Kind::Matrix: return ccc4f(0.78f, 1.00f, 0.82f, 1.f);
            case Kind::Cyber:  return ccc4f(0.82f, 1.00f, 1.00f, 1.f);
            default:           return ccc4f(0.88f, 1.00f, 1.00f, 1.f);
        }
    }
    ccColor4F playCol() { return lightGlass() ? ccc4f(0.86f, 0.38f, 0.30f, 1.f) : ccc4f(1.f, 0.86f, 0.5f, 1.f); }

    // drop shadow behind the labels (themes that float over the gameplay)
    bool hasShadow() { return kind() != Kind::None; }
    ccColor3B shadowCol() {
        switch (kind()) {
            case Kind::Cat:   return ccc3(255, 250, 252);
            case Kind::Cyber: return ccc3(0, 10, 24);
            default:          return ccc3(0, 0, 0);
        }
    }
    GLubyte shadowOpacity() { return kind() == Kind::Matrix ? 215 : kind() == Kind::Cat ? 190 : 175; }
    float   shadowOffset()  { return kind() == Kind::Matrix ? 1.1f : 0.9f; }
}

// ---------- drawing helpers ----------
// CCDrawNode blends premultiplied, so every translucent colour goes through these
static ccColor4F hc(float r, float g, float b, float a = 1.f) { return ccc4f(r * a, g * a, b * a, a); }
static ccColor4F hcc(ccColor4F c, float a) { return hc(c.r, c.g, c.b, a); }

static CCDrawNode* ckyRoundRect(float x, float y, float w, float h, float r,
                                ccColor4F fill, float bw, ccColor4F border) {
    std::vector<CCPoint> v;
    int const seg = 7;
    float const pi = 3.14159265f;
    CCPoint centers[4] = {
        { x + w - r, y + h - r }, { x + r, y + h - r },
        { x + r, y + r },         { x + w - r, y + r }
    };
    for (int c = 0; c < 4; ++c) {
        float a0 = c * 90.f;
        for (int i = 0; i <= seg; ++i) {
            float a = (a0 + 90.f * static_cast<float>(i) / seg) * pi / 180.f;
            v.push_back({ centers[c].x + std::cos(a) * r, centers[c].y + std::sin(a) * r });
        }
    }
    auto dn = CCDrawNode::create();
    dn->drawPolygon(v.data(), static_cast<unsigned int>(v.size()), fill, bw, border);
    return dn;
}

static void ckyRect(CCDrawNode* d, float x0, float y0, float x1, float y1, ccColor4F c) {
    CCPoint p[4] = { {x0, y0}, {x1, y0}, {x1, y1}, {x0, y1} };
    d->drawPolygon(p, 4, c, 0.f, c);
}

static void ckyTri(CCDrawNode* d, CCPoint a, CCPoint b, CCPoint c, ccColor4F col) {
    CCPoint p[3] = { a, b, c };
    d->drawPolygon(p, 3, col, 0.f, col);
}

// filled circle outline built from a polygon (so the fill can be fully transparent: a true ring)
static void ckyCircle(CCDrawNode* d, CCPoint c, float r, ccColor4F fill, float bw, ccColor4F border) {
    CCPoint v[28];
    for (int i = 0; i < 28; ++i) {
        float a = 6.2831853f * static_cast<float>(i) / 28.f;
        v[i] = CCPoint(c.x + std::cos(a) * r, c.y + std::sin(a) * r);
    }
    d->drawPolygon(v, 28, fill, bw, border);
}

static CCPoint ckyP(float p[8][2], int i) { return { p[i][0], p[i][1] }; }

// ---------- panels ----------
// One pill-shaped panel for the current theme, lower-left corner (x, y). `edge` is the accent colour of the
// state (normal / frozen / off). Every theme has a panel; its fill follows the HUD opacity setting.
static CCNode* ckyPanel(float x, float y, float w, float h, ccColor4F edge, uint32_t seed) {
    auto const& l = L();
    float const a = hudA();      // fill opacity (the HUD opacity setting)
    float const e = edgeA();     // outlines / decoration
    switch (kind()) {
        case Kind::Matrix: {
            // black terminal box (alpha = HUD opacity) with a hairline border in the state colour
            auto d = CCDrawNode::create();
            CCPoint v[4] = { { x, y }, { x + w, y }, { x + w, y + h }, { x, y + h } };
            d->drawPolygon(v, 4, hc(0.f, 0.f, 0.f, a), l.border, hcc(edge, e));
            return d;
        }

        case Kind::Cyber: {
            // deep-blue glass, 0.7 pt chamfered cyan outline
            float c = std::min(l.radius, h * 0.18f);
            float p[8][2];
            fx::chamfer(x, y, x + w, y + h, c, p);
            CCPoint v[8];
            for (int i = 0; i < 8; ++i) v[i] = ckyP(p, i);
            auto d = CCDrawNode::create();
            d->drawPolygon(v, 8, hc(0.02f, 0.05f, 0.12f, a), l.border, hcc(edge, e));
            return d;
        }

        case Kind::Cat: {
            // pink-white glass, soft sheen on the top half, hairline peach border + white rim
            auto n = CCNode::create();
            n->addChild(ckyRoundRect(x, y, w, h, l.radius, hc(1.f, 0.957f, 0.969f, a), l.border, hcc(edge, e)));
            auto d = CCDrawNode::create();
            ckyRect(d, x + l.radius * 0.7f, y + h * 0.52f, x + w - l.radius * 0.7f, y + h - 1.6f, hc(1.f, 1.f, 1.f, 0.30f * a));
            n->addChild(d);
            n->addChild(ckyRoundRect(x + 1.3f, y + 1.3f, w - 2.6f, h - 2.6f, std::max(1.f, l.radius - 1.3f),
                                     hc(0.f, 0.f, 0.f, 0.f), 0.35f, hc(1.f, 1.f, 1.f, 0.55f * e)));
            return n;
        }

        case Kind::Lava: {
            // charcoal panel (alpha = HUD opacity), glowing cracked-lava texture, cracked-obsidian border with lava in the gaps
            float c = 7.f;
            float p[8][2];
            fx::chamfer(x, y, x + w, y + h, c, p);
            CCPoint v[8];
            for (int i = 0; i < 8; ++i) v[i] = ckyP(p, i);
            auto d = CCDrawNode::create();
            d->drawPolygon(v, 8, hc(0.11f, 0.063f, 0.047f, a), 0.f, hc(0.f, 0.f, 0.f, 0.f));

            // heat rising from the bottom (stacked translucent bands = a cheap gradient)
            for (int k = 0; k < 4; ++k)
                ckyRect(d, x + c, y, x + w - c, y + h * (0.12f + 0.09f * static_cast<float>(k)), hc(1.f, 0.42f, 0.08f, 0.05f * e));

            // cracks: soft glow underneath, bright core on top, a hot dot where cracks bend
            ccColor4F core = ccc4f(fx::lerp(edge.r, 1.f, 0.4f), fx::lerp(edge.g, 0.82f, 0.4f), fx::lerp(edge.b, 0.4f, 0.4f), 1.f);
            fx::crackNet(w, h, seed, 8.f, [&](float x0, float y0, float x1, float y1, bool br) {
                d->drawSegment(ccp(x + x0, y + y0), ccp(x + x1, y + y1), br ? 1.1f : 1.7f, hcc(edge, 0.14f * e));
                d->drawSegment(ccp(x + x0, y + y0), ccp(x + x1, y + y1), br ? 0.3f : 0.5f, hcc(core, (br ? 0.55f : 0.85f) * e));
                if (!br) d->drawDot(ccp(x + x0, y + y0), 0.7f, hcc(core, 0.55f * e));
            });

            // border: lava line underneath ...
            for (int i = 0; i < 8; ++i) {
                CCPoint pa = ckyP(p, i), pb = ckyP(p, (i + 1) % 8);
                d->drawSegment(pa, pb, 2.8f, hcc(edge, 0.12f));
                d->drawSegment(pa, pb, 1.5f, hcc(edge, 0.60f));
                d->drawSegment(pa, pb, 0.55f, hcc(core, 0.95f));
            }
            // ... cracked obsidian pieces on top; the gaps between them are where the lava shows through
            for (int i = 0; i < 8; ++i) {
                fx::brokenEdge(p[i][0], p[i][1], p[(i + 1) % 8][0], p[(i + 1) % 8][1], seed * 31u + static_cast<uint32_t>(i),
                               9.f, 24.f, 3.4f, 6.f, [&](float sx, float sy, float ex, float ey) {
                    d->drawSegment(ccp(sx, sy), ccp(ex, ey), 1.15f, hc(0.045f, 0.04f, 0.05f, 0.97f));
                    d->drawSegment(ccp(sx, sy), ccp(ex, ey), 0.28f, hc(0.38f, 0.33f, 0.44f, 0.55f));
                });
            }
            for (int i = 0; i < 8; ++i) d->drawDot(ckyP(p, i), 1.3f, hc(0.045f, 0.04f, 0.05f, 0.97f));
            return d;
        }

        default:
            return ckyRoundRect(x, y, w, h, l.radius, themePanel(), l.border, edge);
    }
}

// the round face of a button / clock: ring + disc, drawn per theme
static void ckyDisc(CCDrawNode* d, float D, ccColor4F ring) {
    float c = D / 2.f;
    switch (kind()) {
        case Kind::Matrix:   // thin ring, the black panel is behind it
            ckyCircle(d, { c, c }, c - 0.8f, hc(0.f, 0.f, 0.f, 0.f), 0.6f, ring);
            break;
        case Kind::Cyber:
            ckyCircle(d, { c, c }, c - 0.8f, hc(0.02f, 0.08f, 0.14f, 0.40f + 0.60f * hudA()), 0.6f, ring);
            break;
        case Kind::Cat:
            ckyCircle(d, { c, c }, c - 0.8f, hc(1.f, 0.90f, 0.88f, 0.50f + 0.50f * hudA()), 0.7f, ring);
            break;
        default:
            d->drawDot({ c, c }, c - 1.f, ring);
            d->drawDot({ c, c }, c - 2.4f, themeBtnBg());
            break;
    }
}

// round button face. Glyphs are added by the caller (their sizes scale with the button).
static CCNode* ckyBtnFace(ccColor4F ring) {
    float D = L().btnD;
    auto n = CCNode::create();
    n->setContentSize({ D, D });
    n->setAnchorPoint({ 0.5f, 0.5f });
    auto d = CCDrawNode::create();
    ckyDisc(d, D, ring);
    n->addChild(d);
    return n;
}

static CCDrawNode* ckyGlyphNode(CCNode* face, int tag = 0) {
    auto g = CCDrawNode::create();
    if (tag) g->setTag(tag);
    face->addChild(g);
    return g;
}

static CCNode* ckyPauseBtn() {
    auto n = ckyBtnFace(themeHi());
    float c = L().btnD / 2.f, k = L().btnD / 28.f;
    auto pause = ckyGlyphNode(n, TagGlyphA);
    ckyRect(pause, c - 5.f * k, c - 5.5f * k, c - 1.6f * k, c + 5.5f * k, glyphCol());
    ckyRect(pause, c + 1.6f * k, c - 5.5f * k, c + 5.f * k, c + 5.5f * k, glyphCol());
    auto play = ckyGlyphNode(n, TagGlyphB);
    ckyTri(play, { c - 3.2f * k, c - 6.f * k }, { c - 3.2f * k, c + 6.f * k }, { c + 6.f * k, c }, playCol());
    play->setVisible(false);
    return n;
}

static CCNode* ckyStepBtn() {
    auto n = ckyBtnFace(themeHi());
    float c = L().btnD / 2.f, k = L().btnD / 28.f;
    auto g = ckyGlyphNode(n);
    ckyTri(g, { c - 6.f * k, c - 6.f * k }, { c - 6.f * k, c + 6.f * k }, { c + 3.f * k, c }, glyphCol());
    ckyRect(g, c + 4.2f * k, c - 6.f * k, c + 6.6f * k, c + 6.f * k, glyphCol());
    return n;
}

// back step: mirrored "step" glyph (bar on the left, triangle pointing left)
static CCNode* ckyBackBtn() {
    auto n = ckyBtnFace(themeHi());
    float c = L().btnD / 2.f, k = L().btnD / 28.f;
    auto g = ckyGlyphNode(n);
    ckyTri(g, { c + 6.f * k, c - 6.f * k }, { c + 6.f * k, c + 6.f * k }, { c - 3.f * k, c }, glyphCol());
    ckyRect(g, c - 6.6f * k, c - 6.f * k, c - 4.2f * k, c + 6.f * k, glyphCol());
    return n;
}

static CCNode* ckySignBtn(bool plus) {
    auto n = ckyBtnFace(themeHi());
    float c = L().btnD / 2.f, k = L().btnD / 28.f;
    auto g = ckyGlyphNode(n);
    ckyRect(g, c - 5.5f * k, c - 1.3f * k, c + 5.5f * k, c + 1.3f * k, glyphCol());
    if (plus) ckyRect(g, c - 1.3f * k, c - 5.5f * k, c + 1.3f * k, c + 5.5f * k, glyphCol());
    return n;
}

// themed clock (speedhack toggle). Child tag 91 = ON (theme colour), tag 92 = OFF (grey)
static CCNode* ckyThemedClock() {
    float s = L().btnD, k = s / 28.f;
    auto n = CCNode::create();
    n->setContentSize({ s, s });
    n->setAnchorPoint({ 0.5f, 0.5f });
    float c = s / 2.f;
    auto build = [&](ccColor4F col, int t) {
        auto d = CCDrawNode::create();
        ckyDisc(d, s, col);
        d->drawSegment({ c, c }, { c, c + c * 0.55f }, 1.3f * k, glyphCol());
        d->drawSegment({ c, c }, { c + c * 0.42f, c }, 1.3f * k, glyphCol());
        d->drawDot({ c, c }, 1.8f * k, glyphCol());
        d->setTag(t);
        n->addChild(d);
        return d;
    };
    build(themeHi(), 91);
    build(kGrey, 92)->setVisible(false);
    return n;
}

// ---------- labels (each one can have a drop shadow label at tag + ShadowOff) ----------
template <class F>
static void ckyLbl(CCNode* parent, int tag, F&& f) {
    if (!parent) return;
    if (auto l = typeinfo_cast<CCLabelBMFont*>(parent->getChildByTag(tag))) f(l, false);
    if (auto s = typeinfo_cast<CCLabelBMFont*>(parent->getChildByTag(tag + ShadowOff))) f(s, true);
}

static void ckySetLabel(CCNode* parent, int tag, std::string const& s, ccColor3B col) {
    ckyLbl(parent, tag, [&](CCLabelBMFont* l, bool shadow) {
        l->setString(s.c_str());
        if (!shadow) l->setColor(col);
    });
}

static void ckySetLabelX(CCNode* parent, int tag, float x) {
    ckyLbl(parent, tag, [&](CCLabelBMFont* l, bool shadow) {
        l->setPositionX(shadow ? x + shadowOffset() : x);
    });
}

// shrink a label (never grow it past its base scale) so it always fits inside the panel
static void ckyFitLabel(CCNode* parent, int tag, float baseScale, float maxW) {
    float w = 0.f;
    if (parent)
        if (auto l = typeinfo_cast<CCLabelBMFont*>(parent->getChildByTag(tag))) w = l->getContentSize().width;
    float sc = (w > 1.f) ? std::min(baseScale, maxW / w) : baseScale;
    ckyLbl(parent, tag, [&](CCLabelBMFont* l, bool) { l->setScale(sc); });
}

class CkyHud : public CCNode {
public:
    int m_theme = 0;
    float m_op = 0.f;               // the HUD opacity this HUD was drawn with (CCDrawNode can't change it afterwards)
    fx::EmberPool m_pool[2];        // Molten rocks only: one for each panel
    CCDrawNode*   m_em[2] = { nullptr, nullptr };

    static CkyHud* create() {
        auto r = new CkyHud();
        if (r && r->init()) { r->autorelease(); return r; }
        CC_SAFE_DELETE(r);
        return nullptr;
    }

    void addBtn(CCMenu* m, CCNode* img, SEL_MenuHandler cb, int t, float x, float y) {
        auto b = CCMenuItemSpriteExtra::create(img, this, cb);
        b->setTag(t);
        b->setPosition({ x, y });
        m->addChild(b);
    }

    CCLabelBMFont* addLabel(CCNode* parent, int t, float scale, CCPoint pos,
                            CCPoint anchor = { 0.5f, 0.5f }) {
        if (hasShadow()) {   // drawn first, so it sits behind the text
            auto s = CCLabelBMFont::create("", "bigFont.fnt");
            s->setScale(scale);
            s->setTag(t + ShadowOff);
            s->setAnchorPoint(anchor);
            s->setPosition({ pos.x + shadowOffset(), pos.y - shadowOffset() });
            s->setColor(shadowCol());
            s->setOpacity(shadowOpacity());
            parent->addChild(s);
        }
        auto l = CCLabelBMFont::create("", "bigFont.fnt");
        l->setScale(scale);
        l->setTag(t);
        l->setAnchorPoint(anchor);
        l->setPosition(pos);
        parent->addChild(l);
        return l;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-hud");
        m_theme = themeIdx();
        m_op = hudA();
        auto win = CCDirector::get()->getWinSize();
        this->setPosition({ 0.f, 0.f });

        auto const& l = L();
        float const rowY = l.rowY;
        bool const lava = kind() == Kind::Lava;

        // ---------- top-left: frame stepper (origin = pill's top-left corner) ----------
        auto sg = CCNode::create();
        sg->setID("cky-hud-step-group");
        sg->setPosition({ l.margin, win.height - l.margin });
        this->addChild(sg);

        if (auto sp1 = ckyPanel(0.f, -l.pillH, l.stepW, l.pillH, themeHi(), 0x51u)) {
            sp1->setTag(TagPillA);
            sg->addChild(sp1);
        }
        if (auto sp2 = ckyPanel(0.f, -l.pillH, l.stepW, l.pillH, kFrozen, 0x51u)) {
            sp2->setTag(TagPillB);
            sp2->setVisible(false);
            sg->addChild(sp2);
        }
        if (lava) {
            m_em[0] = CCDrawNode::create();
            sg->addChild(m_em[0]);
        }

        auto sm = CCMenu::create();
        sm->setID("cky-hud-step-menu");
        sm->setPosition({ 0.f, 0.f });
        sg->addChild(sm);
        addBtn(sm, ckyPauseBtn(), menu_selector(CkyHud::onFreeze), HudFreeze, stepBtnX(0), rowY);
        addBtn(sm, ckyBackBtn(), menu_selector(CkyHud::onBack), HudBack, stepBtnX(1), rowY);
        addBtn(sm, ckyStepBtn(), menu_selector(CkyHud::onStep), HudStep, stepBtnX(2), rowY);
        addLabel(sg, HudStepLabel, l.lblStep, { stepLabelX(true, true), rowY }, { 0.f, 0.5f });
        addLabel(sg, HudStepInfo, l.lblInfo, { l.infoX, l.infoY }, { 0.f, 0.5f })->setOpacity(190);

        // ---------- top-right: speedhack (origin = pill's top-right corner) ----------
        auto pg = CCNode::create();
        pg->setID("cky-hud-speed-group");
        pg->setPosition({ win.width - l.margin, win.height - l.margin });
        this->addChild(pg);

        if (auto pp1 = ckyPanel(-l.speedW, -l.pillH, l.speedW, l.pillH, themeHi(), 0x7Au)) {
            pp1->setTag(TagPillA);
            pg->addChild(pp1);
        }
        if (auto pp2 = ckyPanel(-l.speedW, -l.pillH, l.speedW, l.pillH, kGrey, 0x7Au)) {
            pp2->setTag(TagPillB);
            pp2->setVisible(false);
            pg->addChild(pp2);
        }
        if (lava) {
            m_em[1] = CCDrawNode::create();
            pg->addChild(m_em[1]);
        }

        auto pm = CCMenu::create();
        pm->setID("cky-hud-speed-menu");
        pm->setPosition({ 0.f, 0.f });
        pg->addChild(pm);
        addBtn(pm, ckyThemedClock(), menu_selector(CkyHud::onClock), HudClock, speedClockX(), rowY);
        addBtn(pm, ckySignBtn(false), menu_selector(CkyHud::onSpeedDown), HudSpeedDown, speedMinusX(), rowY);
        addBtn(pm, ckySignBtn(true), menu_selector(CkyHud::onSpeedUp), HudSpeedUp, speedPlusX(), rowY);
        addLabel(pg, HudSpeedLabel, l.lblSpeed, { speedLabelX(true), rowY });
        addLabel(pg, HudSpeedInfo, l.lblInfo, { -l.speedW / 2.f, l.infoY })->setOpacity(190);

        // big noclip accuracy (top center)
        addLabel(this, HudNcLabel, 0.85f, { win.width / 2.f, win.height - 70.f });
        addLabel(this, HudNcLabel + 1, 0.4f, { win.width / 2.f, win.height - 94.f });

        if (lava) {
            // more embers than the menu used to have, tuned for a small panel
            for (auto& pool : m_pool) { pool.cap = 14; pool.rate = 16.f; pool.lifeScale = 0.55f; }
            this->scheduleUpdate();
        }
        return true;
    }

    // Molten rocks: embers float up from the bottom edge of both panels, never leaving them.
    // 14 embers at most per panel, two draw calls each; nothing is allocated.
    void tickEmbers(int i, char const* groupId, float x0, float x1, float dt) {
        auto dn = m_em[i];
        if (!dn) return;
        dn->clear();
        auto g = this->getChildByID(groupId);
        if (!g || !g->isVisible()) return;
        auto const& l = L();
        fx::Rect box{ x0 + 2.f, -l.pillH + 1.5f, x1 - 2.f, -1.5f };
        m_pool[i].step(dt, 0.75f, box);
        for (auto const& q : m_pool[i].p) {
            if (!q.alive) continue;
            float a = fx::EmberPool::alphaOf(q);
            fx::Col col = fx::emberColor(q.age / q.life);
            dn->drawDot(ccp(q.x, q.y), q.size * 2.2f, hc(col.r, col.g, col.b, a * 0.16f));
            dn->drawDot(ccp(q.x, q.y), q.size, hc(col.r, col.g, col.b, a * 0.95f));
        }
    }

    void update(float dt) override {
        if (!this->isVisible()) return;
        dt = std::min(dt, 0.05f);
        auto const& l = L();
        tickEmbers(0, "cky-hud-step-group", 0.f, l.stepW, dt);
        tickEmbers(1, "cky-hud-speed-group", -l.speedW, 0.f, dt);
    }

    void onFreeze(CCObject*) {
        cky::frozen = !cky::frozen;
        cky::pending = 0;
        ckyHudRefresh();
        ckyApplyAudio();
    }

    void onStep(CCObject*) {
        if (!cky::frozen) {
            cky::frozen = true;
            cky::pending = 0;
            ckyHudRefresh();
            ckyApplyAudio();
            return;
        }
        if (cky::pending < 100) cky::pending = cky::pending + 1;
    }

    void onBack(CCObject*) {
        if (!cky::frozen) { // first press just pauses, like the step button
            cky::frozen = true;
            cky::pending = 0;
            ckyHudRefresh();
            ckyApplyAudio();
            return;
        }
        if (!cky::backStepOn) { ckyNotify("Back step is off", false); return; }
        if (!ckyBackStep()) ckyNotify("Nothing further back", false);
        ckyHudRefresh();
    }

    void onClock(CCObject*) {
        cky::speedOn = !cky::speedOn;
        ckyApplyAudio();
        ckyHudRefresh();
    }

    void onSpeedDown(CCObject*) {
        float cur = cky::speed;
        float best = -1.f;
        for (float v : cky::speedList) if (v < cur - 0.001f) best = std::max(best, v);
        if (best < 0.f) best = std::max(0.01f, std::round(cur * 50.f) / 100.f); // below the list: halve
        cky::speed = best;
        cky::speedOn = true;
        ckyApplyAudio();
        ckyHudRefresh();
    }

    void onSpeedUp(CCObject*) {
        float cur = cky::speed;
        float best = 1e9f;
        for (float v : cky::speedList) if (v > cur + 0.001f) best = std::min(best, v);
        if (best > 1e8f) best = cur;
        cky::speed = std::min(4.f, best);
        cky::speedOn = true;
        ckyApplyAudio();
        ckyHudRefresh();
    }
};

CCNode* ckyHudNode() {
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_uiLayer) return nullptr;
    return pl->m_uiLayer->getChildByID("cky-hud");
}

static void ckyScaleGroups(CCNode* hud) {
    if (!hud) return;
    for (auto id : { "cky-hud-step-group", "cky-hud-speed-group" }) {
        if (auto g = hud->getChildByID(id)) g->setScale(cky::hudScale);
    }
}

void ckyApplyHudScale() { ckyScaleGroups(ckyHudNode()); }

void ckyHudRefresh(CCNode* hud) {
    if (!hud) hud = ckyHudNode();
    if (!hud) return;

    // menu theme or HUD opacity changed -> rebuild the HUD (the panels are drawn shapes, so they are redrawn)
    if (auto h = typeinfo_cast<CkyHud*>(hud)) {
        if (h->m_theme != themeIdx() || std::abs(h->m_op - hudA()) > 0.001f) {
            if (auto pl = PlayLayer::get()) { ckyBuildHud(pl); return; }
        }
    }

    auto const& l = L();
    ccColor3B const white = textMain();
    ccColor3B const dim   = textDim();
    ccColor3B const warn  = textWarn();
    ccColor3B const infoCol = textInfo();

    // ---- stepper ----
    if (auto sg = hud->getChildByID("cky-hud-step-group")) {
        sg->setVisible(cky::hudStepShow);
        bool sb = cky::stepperBtns, bk = cky::backStepOn;
        {   // buttons are removed (not just disabled) when switched off
            if (auto sm = sg->getChildByID("cky-hud-step-menu")) {
                sm->setVisible(sb);
                if (auto b = sm->getChildByTag(HudBack)) b->setVisible(bk);
                if (auto s = sm->getChildByTag(HudStep)) s->setPositionX(stepBtnX(bk ? 2 : 1));
            }
            ckySetLabelX(sg, HudStepLabel, stepLabelX(sb, bk));
        }
        std::string mode = cky::recording ? (cky::recArmed ? "REC" : "REC-wait")
                         : cky::playing   ? (cky::playArmed ? "PLAY" : "PLAY-wait") : "";
        ckySetLabel(sg, HudStepLabel, fmt::format("Frame {}", cky::step), cky::frozen ? warn : white);
        ckyFitLabel(sg, HudStepLabel, l.lblStep, l.stepW - stepLabelX(sb, bk) - 4.f);

        std::string info = cky::frozen ? "PAUSED | " : "";
        if (!mode.empty()) info += mode + " | ";
        info += fmt::format("In {} | CP {}", cky::macro.size(), cky::ckpts.size());
        if (cky::backStepOn) info += fmt::format(" | BK {}", ckyBackStepCount());
        ckySetLabel(sg, HudStepInfo, info, cky::frozen ? warn : infoCol);
        ckyFitLabel(sg, HudStepInfo, l.lblInfo, l.stepW - l.infoX * 2.f);

        if (auto a = sg->getChildByTag(TagPillA)) a->setVisible(!cky::frozen);
        if (auto b = sg->getChildByTag(TagPillB)) b->setVisible(cky::frozen);

        if (auto sm = sg->getChildByID("cky-hud-step-menu")) {
            if (auto b = typeinfo_cast<CCMenuItemSpriteExtra*>(sm->getChildByTag(HudFreeze))) {
                if (auto img = b->getNormalImage()) {
                    if (auto p = img->getChildByTag(TagGlyphA)) p->setVisible(!cky::frozen);
                    if (auto p = img->getChildByTag(TagGlyphB)) p->setVisible(cky::frozen);
                }
            }
        }
    }

    // ---- speedhack ----
    if (auto pg = hud->getChildByID("cky-hud-speed-group")) {
        pg->setVisible(cky::hudSpeedShow);
        if (auto pm = pg->getChildByID("cky-hud-speed-menu")) pm->setVisible(cky::speedBtns);
        ckySetLabelX(pg, HudSpeedLabel, speedLabelX(cky::speedBtns));
        bool on = cky::speedOn;
        ckySetLabel(pg, HudSpeedLabel, fmt::format("{:.2f}x", cky::effSpeed()), on ? white : dim);
        ckyFitLabel(pg, HudSpeedLabel, l.lblSpeed,
                    cky::speedBtns ? (speedPlusX() - speedMinusX()) - l.btnD - 2.f : l.speedW - 8.f);

        std::string real = cky::actualTps > 0.f
            ? fmt::format("{}", static_cast<int>(cky::actualTps + 0.5f))
            : std::string("-");
        // real TPS colour: green when it matches the target, orange when it falls short
        ccColor3B realCol = infoCol;
        if (cky::actualTps > 0.f) {
            float ratio = cky::actualTps / std::max(1.f, cky::effTps() * cky::effSpeed());
            realCol = ratio >= 0.97f ? textOk() : textAlert();
        }
        ckySetLabel(pg, HudSpeedInfo,
            fmt::format("{} TPS | real {}", static_cast<int>(cky::effTps()), real),
            on ? realCol : dim);
        ckyFitLabel(pg, HudSpeedInfo, l.lblInfo, l.speedW - l.infoX * 2.f);

        if (auto a = pg->getChildByTag(TagPillA)) a->setVisible(on);
        if (auto b = pg->getChildByTag(TagPillB)) b->setVisible(!on);

        if (auto pm = pg->getChildByID("cky-hud-speed-menu")) {
            if (auto b = typeinfo_cast<CCMenuItemSpriteExtra*>(pm->getChildByTag(HudClock))) {
                if (auto img = b->getNormalImage()) {
                    if (auto o = img->getChildByTag(91)) o->setVisible(on);
                    if (auto o = img->getChildByTag(92)) o->setVisible(!on);
                }
            }
        }
    }

    // ---- noclip accuracy ----
    double a = cky::ncAccuracy();
    ccColor3B accCol = a >= 99.99 ? ccc3(90, 255, 130)
                     : a >= 95.0  ? ccc3(200, 255, 90)
                     : a >= 80.0  ? ccc3(255, 190, 70)
                                  : ccc3(255, 90, 90);
    bool showAcc = cky::noclip && cky::ncShowAcc;
    ckyLbl(hud, HudNcLabel, [&](CCLabelBMFont* lb, bool shadow) {
        lb->setVisible(showAcc);
        lb->setString(fmt::format("{:.2f}%", a).c_str());
        if (!shadow) lb->setColor(accCol);
    });
    bool showDth = cky::noclip && cky::ncShowDeaths;
    ckyLbl(hud, HudNcLabel + 1, [&](CCLabelBMFont* lb, bool) {
        lb->setVisible(showDth);
        lb->setString(fmt::format("Deaths: {}", cky::ncDeaths).c_str());
    });
}

void ckyBuildHud(PlayLayer* pl) {
    if (!pl || !pl->m_uiLayer) return;
    cky::theme = Mod::get()->getSavedValue<int>("theme", cky::theme);
    if (auto old = pl->m_uiLayer->getChildByID("cky-hud")) old->removeFromParent();
    auto hud = CkyHud::create();
    if (!hud) return;
    hud->setVisible(cky::hudOn);
    pl->m_uiLayer->addChild(hud, 100);
    ckyScaleGroups(hud);
    ckyHudRefresh(hud);
}

$on_mod(Loaded) {
    cky::hudOpacityLoad();
}

// END OF FILE (hud.cpp)
