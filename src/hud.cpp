// hud.cpp — in-level HUD: frame stepper (top-left), speedhack (top-right), big accuracy
#include "state.hpp"
#include "icons.hpp"

namespace {
    constexpr int HudStepLabel = 10, HudSpeedLabel = 11, HudNcLabel = 12;
    constexpr int HudFreeze = 1, HudStep = 2, HudSpeedDown = 3, HudSpeedUp = 4, HudClock = 5;
}

class CkyHud : public CCNode {
public:
    static CkyHud* create() {
        auto r = new CkyHud();
        if (r && r->init()) { r->autorelease(); return r; }
        CC_SAFE_DELETE(r);
        return nullptr;
    }

    void addBtn(CCMenu* m, CCNode* img, SEL_MenuHandler cb, int t, float x) {
        auto b = CCMenuItemSpriteExtra::create(img, this, cb);
        b->setTag(t);
        b->setPosition({x, 0.f});
        m->addChild(b);
    }

    CCLabelBMFont* addLabel(CCNode* parent, int t, float scale, CCPoint pos) {
        auto l = CCLabelBMFont::create("", "bigFont.fnt");
        l->setScale(scale);
        l->setTag(t);
        l->setPosition(pos);
        parent->addChild(l);
        return l;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-hud");
        auto win = CCDirector::get()->getWinSize();
        this->setPosition({0.f, 0.f});

        // top-left: frame stepper
        auto sg = CCNode::create();
        sg->setID("cky-hud-step-group");
        sg->setPosition({62.f, win.height - 26.f});
        this->addChild(sg);
        auto sm = CCMenu::create();
        sm->setID("cky-hud-step-menu");
        sm->setPosition({0.f, 0.f});
        sg->addChild(sm);
        auto fr = ButtonSprite::create("Freeze");
        fr->setScale(0.4f);
        addBtn(sm, fr, menu_selector(CkyHud::onFreeze), HudFreeze, -22.f);
        addBtn(sm, ckyArrowIcon(26.f), menu_selector(CkyHud::onStep), HudStep, 28.f);
        addLabel(sg, HudStepLabel, 0.28f, {60.f, -20.f})->setOpacity(200);

        // top-right: speedhack
        auto pg = CCNode::create();
        pg->setID("cky-hud-speed-group");
        pg->setPosition({win.width - 120.f, win.height - 26.f});
        this->addChild(pg);
        auto pm = CCMenu::create();
        pm->setID("cky-hud-speed-menu");
        pm->setPosition({0.f, 0.f});
        pg->addChild(pm);
        addBtn(pm, ckyClockIcon(22.f), menu_selector(CkyHud::onClock), HudClock, -34.f);
        auto dn = ButtonSprite::create("-");
        dn->setScale(0.4f);
        addBtn(pm, dn, menu_selector(CkyHud::onSpeedDown), HudSpeedDown, 0.f);
        auto up = ButtonSprite::create("+");
        up->setScale(0.4f);
        addBtn(pm, up, menu_selector(CkyHud::onSpeedUp), HudSpeedUp, 30.f);
        addLabel(pg, HudSpeedLabel, 0.28f, {-5.f, -20.f})->setOpacity(200);

        // big noclip accuracy (top center)
        addLabel(this, HudNcLabel, 0.85f, {win.width / 2.f, win.height - 70.f});
        addLabel(this, HudNcLabel + 1, 0.4f, {win.width / 2.f, win.height - 94.f});
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

void ckyHudRefresh(CCNode* hud) {
    if (!hud) hud = ckyHudNode();
    if (!hud) return;

    if (auto sg = hud->getChildByID("cky-hud-step-group")) {
        if (auto l = typeinfo_cast<CCLabelBMFont*>(sg->getChildByTag(HudStepLabel))) {
            std::string mode = cky::recording ? (cky::recArmed ? "REC" : "REC-wait")
                             : cky::playing ? (cky::playArmed ? "PLAY" : "PLAY-wait") : "";
            l->setString(fmt::format("Step {} | In {} | CP {} {}",
                cky::step, cky::macro.size(), cky::ckpts.size(), mode).c_str());
        }
        if (auto sm = sg->getChildByID("cky-hud-step-menu")) {
            if (auto b = typeinfo_cast<CCMenuItemSpriteExtra*>(sm->getChildByTag(HudFreeze))) {
                if (auto s = typeinfo_cast<ButtonSprite*>(b->getNormalImage())) {
                    s->setString(cky::frozen ? "Run" : "Freeze");
                }
            }
        }
    }

    if (auto pg = hud->getChildByID("cky-hud-speed-group")) {
        std::string real = cky::actualTps > 0.f
            ? fmt::format("{}", static_cast<int>(cky::actualTps + 0.5f))
            : std::string("-");
        if (auto l = typeinfo_cast<CCLabelBMFont*>(pg->getChildByTag(HudSpeedLabel))) {
            l->setString(fmt::format("{:.2f}x | {} TPS (real {})",
                cky::effSpeed(), static_cast<int>(cky::effTps()), real).c_str());
        }
        if (auto pm = pg->getChildByID("cky-hud-speed-menu")) {
            if (auto b = typeinfo_cast<CCMenuItemSpriteExtra*>(pm->getChildByTag(HudClock))) {
                if (auto img = b->getNormalImage()) {
                    if (auto on = img->getChildByTag(91)) on->setVisible(cky::speedOn);
                    if (auto off = img->getChildByTag(92)) off->setVisible(!cky::speedOn);
                }
            }
        }
    }

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
