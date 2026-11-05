// hud.cpp — in-level HUD: frame stepper (top-left), speedhack (top-right), big accuracy
#include "state.hpp"
#include "icons.hpp"

namespace {
    // label tags
    constexpr int HudStepLabel = 10, HudSpeedLabel = 11, HudNcLabel = 12;
    constexpr int HudStepInfo = 15, HudSpeedInfo = 16;
    // button tags
    constexpr int HudFreeze = 1, HudStep = 2, HudSpeedDown = 3, HudSpeedUp = 4, HudClock = 5;
    // glyph / pill tags
    constexpr int TagPillA = 81, TagPillB = 82;   // pill: normal / alternate look
    constexpr int TagGlyphA = 61, TagGlyphB = 62; // freeze button: pause / play

    constexpr float PillH = 44.f;
    constexpr float StepW = 158.f;
    constexpr float SpeedW = 158.f;
    constexpr float BtnD = 28.f;
    constexpr float Margin = 10.f;

    ccColor4F const kCyan   = { 0.35f, 0.85f, 1.00f, 1.f };
    ccColor4F const kOrange = { 1.00f, 0.72f, 0.25f, 1.f };
    ccColor4F const kGrey   = { 0.45f, 0.45f, 0.52f, 1.f };
    ccColor4F const kGlyph  = { 0.88f, 1.00f, 1.00f, 1.f };
    ccColor4F const kPanel  = { 0.04f, 0.05f, 0.17f, 0.74f };
    ccColor4F const kBtnBg  = { 0.07f, 0.11f, 0.30f, 1.f };
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
    d->drawDot({ c, c }, c - 2.4f, kBtnBg);
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
    auto n = ckyBtnFace(kCyan);
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
    auto n = ckyBtnFace(kCyan);
    float c = BtnD / 2.f;
    auto g = ckyGlyphNode(n);
    ckyTri(g, { c - 6.f, c - 6.f }, { c - 6.f, c + 6.f }, { c + 3.f, c }, kGlyph);
    ckyRect(g, c + 4.2f, c - 6.f, c + 6.6f, c + 6.f, kGlyph);
    return n;
}

static CCNode* ckySignBtn(bool plus) {
    auto n = ckyBtnFace(kCyan);
    float c = BtnD / 2.f;
    auto g = ckyGlyphNode(n);
    ckyRect(g, c - 5.5f, c - 1.3f, c + 5.5f, c + 1.3f, kGlyph);
    if (plus) ckyRect(g, c - 1.3f, c - 5.5f, c + 1.3f, c + 5.5f, kGlyph);
    return n;
}

class CkyHud : public CCNode {
public:
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
        auto win = CCDirector::get()->getWinSize();
        this->setPosition({ 0.f, 0.f });

        float const rowY = -17.f;

        // ---------- top-left: frame stepper (origin = pill's top-left corner) ----------
        auto sg = CCNode::create();
        sg->setID("cky-hud-step-group");
        sg->setPosition({ Margin, win.height - Margin });
        this->addChild(sg);

        auto sp1 = ckyRoundRect(0.f, -PillH, StepW, PillH, 12.f, kPanel, 1.3f, kCyan);
        sp1->setTag(TagPillA);
        sg->addChild(sp1);
        auto sp2 = ckyRoundRect(0.f, -PillH, StepW, PillH, 12.f, kPanel, 1.3f, kOrange);
        sp2->setTag(TagPillB);
        sp2->setVisible(false);
        sg->addChild(sp2);

        auto sm = CCMenu::create();
        sm->setID("cky-hud-step-menu");
        sm->setPosition({ 0.f, 0.f });
        sg->addChild(sm);
        addBtn(sm, ckyPauseBtn(), menu_selector(CkyHud::onFreeze), HudFreeze, 20.f, rowY);
        addBtn(sm, ckyStepBtn(), menu_selector(CkyHud::onStep), HudStep, 52.f, rowY);
        addLabel(sg, HudStepLabel, 0.34f, { 74.f, rowY }, { 0.f, 0.5f });
        addLabel(sg, HudStepInfo, 0.2f, { 10.f, -37.f }, { 0.f, 0.5f })->setOpacity(190);

        // ---------- top-right: speedhack (origin = pill's top-right corner) ----------
        auto pg = CCNode::create();
        pg->setID("cky-hud-speed-group");
        pg->setPosition({ win.width - Margin, win.height - Margin });
        this->addChild(pg);

        auto pp1 = ckyRoundRect(-SpeedW, -PillH, SpeedW, PillH, 12.f, kPanel, 1.3f, kCyan);
        pp1->setTag(TagPillA);
        pg->addChild(pp1);
        auto pp2 = ckyRoundRect(-SpeedW, -PillH, SpeedW, PillH, 12.f, kPanel, 1.3f, kGrey);
        pp2->setTag(TagPillB);
        pp2->setVisible(false);
        pg->addChild(pp2);

        auto pm = CCMenu::create();
        pm->setID("cky-hud-speed-menu");
        pm->setPosition({ 0.f, 0.f });
        pg->addChild(pm);
        addBtn(pm, ckyClockIcon(BtnD - 2.f), menu_selector(CkyHud::onClock), HudClock, -136.f, rowY);
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

    void onClock(CCObject*) {
        cky::speedOn = !cky::speedOn;
        ckyApplyAudio();
        ckyHudRefresh();
    }

    void onSpeedDown(CCObject*) {
        cky::speed = cky::nextVal(cky::speedList, cky::speed, -1);
        cky::speedOn = true;
        ckyApplyAudio();
        ckyHudRefresh();
    }

    void onSpeedUp(CCObject*) {
        cky::speed = cky::nextVal(cky::speedList, cky::speed, 1);
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

    ccColor3B const white = ccc3(255, 255, 255);
    ccColor3B const dim   = ccc3(150, 150, 165);

    // ---- stepper ----
    if (auto sg = hud->getChildByID("cky-hud-step-group")) {
        std::string mode = cky::recording ? (cky::recArmed ? "REC" : "REC-wait")
                         : cky::playing   ? (cky::playArmed ? "PLAY" : "PLAY-wait") : "";
        ckySetLabel(sg, HudStepLabel, fmt::format("Frame {}", cky::step),
            cky::frozen ? ccc3(255, 205, 120) : white);

        std::string info = cky::frozen ? "PAUSED | " : "";
        if (!mode.empty()) info += mode + " | ";
        info += fmt::format("In {} | CP {}", cky::macro.size(), cky::ckpts.size());
        ckySetLabel(sg, HudStepInfo, info, cky::frozen ? ccc3(255, 205, 120) : ccc3(190, 230, 255));

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
        bool on = cky::speedOn;
        ckySetLabel(pg, HudSpeedLabel, fmt::format("{:.2f}x", cky::effSpeed()),
            on ? white : dim);

        std::string real = cky::actualTps > 0.f
            ? fmt::format("{}", static_cast<int>(cky::actualTps + 0.5f))
            : std::string("-");
        // real TPS colour: green when it matches the target, orange when it falls short
        ccColor3B realCol = ccc3(190, 230, 255);
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
    if (auto old = pl->m_uiLayer->getChildByID("cky-hud")) old->removeFromParent();
    auto hud = CkyHud::create();
    if (!hud) return;
    hud->setVisible(cky::hudOn);
    pl->m_uiLayer->addChild(hud, 100);
    ckyScaleGroups(hud);
    ckyHudRefresh(hud);
}

// END OF FILE (hud.cpp)
