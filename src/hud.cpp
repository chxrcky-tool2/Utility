// hud.cpp — in-level HUD: frame stepper (top-left), speedhack (top-right), big accuracy
#include "state.hpp"
#include "icons.hpp"

namespace {
    // label tags
    constexpr int HudStepLabel = 10, HudSpeedLabel = 11, HudNcLabel = 12;
    constexpr int HudStepInfo = 15, HudSpeedInfo = 16;
    // button tags
    constexpr int HudFreeze = 1, HudStep = 2, HudSpeedDown = 3, HudSpeedUp = 4, HudClock = 5, HudBack = 6;
    // glyph / pill tags
    constexpr int TagPillA = 81, TagPillB = 82;   // pill: normal / alternate look
    constexpr int TagGlyphA = 61, TagGlyphB = 62; // freeze button: pause / play

    constexpr float PillH = 44.f;
    constexpr float StepW = 190.f;
    constexpr float SpeedW = 158.f;
    constexpr float BtnD = 28.f;
    constexpr float Margin = 10.f;

    ccColor4F const kGrey   = { 0.45f, 0.45f, 0.52f, 1.f };
    ccColor4F const kGlyph  = { 0.88f, 1.00f, 1.00f, 1.f };
    ccColor4F const kFrozen = { 1.00f, 0.82f, 0.30f, 1.f };

    // same colours as the ImGui / Cocos menu themes: Neon, Ocean, Forest, Sunset, Mono
    ccColor4F const kThemeHi[5] = {
        { 0.93f, 0.30f, 0.50f, 1.f }, { 0.24f, 0.62f, 1.00f, 1.f },
        { 0.30f, 0.76f, 0.40f, 1.f }, { 1.00f, 0.62f, 0.22f, 1.f },
        { 0.72f, 0.72f, 0.76f, 1.f }
    };
    ccColor4F const kThemeLo[5] = {
        { 0.62f, 0.14f, 0.30f, 1.f }, { 0.10f, 0.28f, 0.62f, 1.f },
        { 0.10f, 0.42f, 0.26f, 1.f }, { 0.72f, 0.20f, 0.34f, 1.f },
        { 0.30f, 0.30f, 0.33f, 1.f }
    };

    int themeIdx() { return std::clamp(cky::theme, 0, 4); }
    ccColor4F themeHi() { return kThemeHi[themeIdx()]; }
    ccColor4F themeLo() { return kThemeLo[themeIdx()]; }
    // dark panel tinted with the theme colour
    ccColor4F themePanel() {
        auto lo = themeLo();
        return ccc4f(0.04f + lo.r * 0.22f, 0.04f + lo.g * 0.22f, 0.07f + lo.b * 0.22f, 0.78f);
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
}

// ---------- drawing helpers ----------
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

// round button face (neon ring + dark disc). Glyphs are added by the caller.
static CCNode* ckyBtnFace(ccColor4F ring) {
    auto n = CCNode::create();
    n->setContentSize({ BtnD, BtnD });
    n->setAnchorPoint({ 0.5f, 0.5f });
    float c = BtnD / 2.f;
    auto d = CCDrawNode::create();
    d->drawDot({ c, c }, c - 1.f, ring);
    d->drawDot({ c, c }, c - 2.4f, themeBtnBg());
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
    float c = BtnD / 2.f;
    auto pause = ckyGlyphNode(n, TagGlyphA);
    ckyRect(pause, c - 5.f, c - 5.5f, c - 1.6f, c + 5.5f, kGlyph);
    ckyRect(pause, c + 1.6f, c - 5.5f, c + 5.f, c + 5.5f, kGlyph);
    auto play = ckyGlyphNode(n, TagGlyphB);
    ckyTri(play, { c - 3.2f, c - 6.f }, { c - 3.2f, c + 6.f }, { c + 6.f, c }, ccc4f(1.f, 0.86f, 0.5f, 1.f));
    play->setVisible(false);
    return n;
}

static CCNode* ckyStepBtn() {
    auto n = ckyBtnFace(themeHi());
    float c = BtnD / 2.f;
    auto g = ckyGlyphNode(n);
    ckyTri(g, { c - 6.f, c - 6.f }, { c - 6.f, c + 6.f }, { c + 3.f, c }, kGlyph);
    ckyRect(g, c + 4.2f, c - 6.f, c + 6.6f, c + 6.f, kGlyph);
    return n;
}

// back step: mirrored "step" glyph (bar on the left, triangle pointing left)
static CCNode* ckyBackBtn() {
    auto n = ckyBtnFace(themeHi());
    float c = BtnD / 2.f;
    auto g = ckyGlyphNode(n);
    ckyTri(g, { c + 6.f, c - 6.f }, { c + 6.f, c + 6.f }, { c - 3.f, c }, kGlyph);
    ckyRect(g, c - 6.6f, c - 6.f, c - 4.2f, c + 6.f, kGlyph);
    return n;
}

static CCNode* ckySignBtn(bool plus) {
    auto n = ckyBtnFace(themeHi());
    float c = BtnD / 2.f;
    auto g = ckyGlyphNode(n);
    ckyRect(g, c - 5.5f, c - 1.3f, c + 5.5f, c + 1.3f, kGlyph);
    if (plus) ckyRect(g, c - 1.3f, c - 5.5f, c + 1.3f, c + 5.5f, kGlyph);
    return n;
}

// themed clock (speedhack toggle). Child tag 91 = ON (theme colour), tag 92 = OFF (grey)
static CCNode* ckyThemedClock() {
    float s = BtnD;
    auto n = CCNode::create();
    n->setContentSize({ s, s });
    n->setAnchorPoint({ 0.5f, 0.5f });
    float c = s / 2.f;
    auto build = [&](ccColor4F col, int t) {
        auto d = CCDrawNode::create();
        d->drawDot({ c, c }, c - 1.f, col);
        d->drawDot({ c, c }, c - 2.4f, themeBtnBg());
        d->drawSegment({ c, c }, { c, c + c * 0.55f }, 1.3f, kGlyph);
        d->drawSegment({ c, c }, { c + c * 0.42f, c }, 1.3f, kGlyph);
        d->drawDot({ c, c }, 1.8f, kGlyph);
        d->setTag(t);
        n->addChild(d);
        return d;
    };
    build(themeHi(), 91);
    build(kGrey, 92)->setVisible(false);
    return n;
}

class CkyHud : public CCNode {
public:
    int m_theme = 0;
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
        auto win = CCDirector::get()->getWinSize();
        this->setPosition({ 0.f, 0.f });

        float const rowY = -17.f;

        // ---------- top-left: frame stepper (origin = pill's top-left corner) ----------
        auto sg = CCNode::create();
        sg->setID("cky-hud-step-group");
        sg->setPosition({ Margin, win.height - Margin });
        this->addChild(sg);

        auto sp1 = ckyRoundRect(0.f, -PillH, StepW, PillH, 12.f, themePanel(), 1.6f, themeHi());
        sp1->setTag(TagPillA);
        sg->addChild(sp1);
        auto sp2 = ckyRoundRect(0.f, -PillH, StepW, PillH, 12.f, themePanel(), 1.6f, kFrozen);
        sp2->setTag(TagPillB);
        sp2->setVisible(false);
        sg->addChild(sp2);

        auto sm = CCMenu::create();
        sm->setID("cky-hud-step-menu");
        sm->setPosition({ 0.f, 0.f });
        sg->addChild(sm);
        addBtn(sm, ckyPauseBtn(), menu_selector(CkyHud::onFreeze), HudFreeze, 20.f, rowY);
        addBtn(sm, ckyBackBtn(), menu_selector(CkyHud::onBack), HudBack, 52.f, rowY);
        addBtn(sm, ckyStepBtn(), menu_selector(CkyHud::onStep), HudStep, 84.f, rowY);
        addLabel(sg, HudStepLabel, 0.34f, { 106.f, rowY }, { 0.f, 0.5f });
        addLabel(sg, HudStepInfo, 0.2f, { 10.f, -37.f }, { 0.f, 0.5f })->setOpacity(190);

        // ---------- top-right: speedhack (origin = pill's top-right corner) ----------
        auto pg = CCNode::create();
        pg->setID("cky-hud-speed-group");
        pg->setPosition({ win.width - Margin, win.height - Margin });
        this->addChild(pg);

        auto pp1 = ckyRoundRect(-SpeedW, -PillH, SpeedW, PillH, 12.f, themePanel(), 1.6f, themeHi());
        pp1->setTag(TagPillA);
        pg->addChild(pp1);
        auto pp2 = ckyRoundRect(-SpeedW, -PillH, SpeedW, PillH, 12.f, themePanel(), 1.6f, kGrey);
        pp2->setTag(TagPillB);
        pp2->setVisible(false);
        pg->addChild(pp2);

        auto pm = CCMenu::create();
        pm->setID("cky-hud-speed-menu");
        pm->setPosition({ 0.f, 0.f });
        pg->addChild(pm);
        addBtn(pm, ckyThemedClock(), menu_selector(CkyHud::onClock), HudClock, -136.f, rowY);
        addBtn(pm, ckySignBtn(false), menu_selector(CkyHud::onSpeedDown), HudSpeedDown, -104.f, rowY);
        addBtn(pm, ckySignBtn(true), menu_selector(CkyHud::onSpeedUp), HudSpeedUp, -22.f, rowY);
        addLabel(pg, HudSpeedLabel, 0.42f, { -63.f, rowY });
        addLabel(pg, HudSpeedInfo, 0.2f, { -SpeedW / 2.f, -37.f })->setOpacity(190);

        // big noclip accuracy (top center)
        addLabel(this, HudNcLabel, 0.85f, { win.width / 2.f, win.height - 70.f });
        addLabel(this, HudNcLabel + 1, 0.4f, { win.width / 2.f, win.height - 94.f });
        return true;
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

static void ckySetLabel(CCNode* parent, int tag, std::string const& s, ccColor3B col) {
    if (auto l = typeinfo_cast<CCLabelBMFont*>(parent->getChildByTag(tag))) {
        l->setString(s.c_str());
        l->setColor(col);
    }
}

void ckyHudRefresh(CCNode* hud) {
    if (!hud) hud = ckyHudNode();
    if (!hud) return;

    // menu theme changed -> rebuild the HUD in the new colours
    if (auto h = typeinfo_cast<CkyHud*>(hud)) {
        if (h->m_theme != themeIdx()) {
            if (auto pl = PlayLayer::get()) { ckyBuildHud(pl); return; }
        }
    }

    ccColor3B const white = ccc3(255, 255, 255);
    ccColor3B const dim   = ccc3(150, 150, 165);
    ccColor4F hi = themeHi();
    ccColor3B infoCol = toC3(ccc4f(0.5f + hi.r * 0.5f, 0.5f + hi.g * 0.5f, 0.5f + hi.b * 0.5f, 1.f));

    // ---- stepper ----
    if (auto sg = hud->getChildByID("cky-hud-step-group")) {
        sg->setVisible(cky::hudStepShow);
        std::string mode = cky::recording ? (cky::recArmed ? "REC" : "REC-wait")
                         : cky::playing   ? (cky::playArmed ? "PLAY" : "PLAY-wait") : "";
        ckySetLabel(sg, HudStepLabel, fmt::format("Frame {}", cky::step),
            cky::frozen ? ccc3(255, 205, 120) : white);

        std::string info = cky::frozen ? "PAUSED | " : "";
        if (!mode.empty()) info += mode + " | ";
        info += fmt::format("In {} | CP {}", cky::macro.size(), cky::ckpts.size());
        if (cky::backStepOn) info += fmt::format(" | BK {}", ckyBackStepCount());
        ckySetLabel(sg, HudStepInfo, info, cky::frozen ? ccc3(255, 205, 120) : infoCol);

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
        bool on = cky::speedOn;
        ckySetLabel(pg, HudSpeedLabel, fmt::format("{:.2f}x", cky::effSpeed()),
            on ? white : dim);

        std::string real = cky::actualTps > 0.f
            ? fmt::format("{}", static_cast<int>(cky::actualTps + 0.5f))
            : std::string("-");
        // real TPS colour: green when it matches the target, orange when it falls short
        ccColor3B realCol = infoCol;
        if (cky::actualTps > 0.f) {
            float ratio = cky::actualTps / std::max(1.f, cky::effTps());
            realCol = ratio >= 0.97f ? ccc3(120, 255, 150) : ccc3(255, 190, 90);
        }
        ckySetLabel(pg, HudSpeedInfo,
            fmt::format("{} TPS | real {}", static_cast<int>(cky::effTps()), real),
            on ? realCol : dim);

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
    auto acc = typeinfo_cast<CCLabelBMFont*>(hud->getChildByTag(HudNcLabel));
    auto dth = typeinfo_cast<CCLabelBMFont*>(hud->getChildByTag(HudNcLabel + 1));
    if (acc) {
        double a = cky::ncAccuracy();
        acc->setVisible(cky::noclip && cky::ncShowAcc);
        acc->setString(fmt::format("{:.2f}%", a).c_str());
        acc->setColor(a >= 99.99 ? ccc3(90, 255, 130)
                    : a >= 95.0  ? ccc3(200, 255, 90)
                    : a >= 80.0  ? ccc3(255, 190, 70)
                                 : ccc3(255, 90, 90));
    }
    if (dth) {
        dth->setVisible(cky::noclip && cky::ncShowDeaths);
        dth->setString(fmt::format("Deaths: {}", cky::ncDeaths).c_str());
    }
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

// END OF FILE (hud.cpp)
