#include "state.hpp"

class CkyHud : public cocos2d::CCNode {
public:
    cocos2d::CCLabelBMFont* m_label = nullptr;
    static CkyHud* create() { auto ret = new CkyHud(); if (ret && ret->init()) { ret->autorelease(); return ret; } CC_SAFE_DELETE(ret); return nullptr; }
    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-hud");
        m_label = cocos2d::CCLabelBMFont::create("0 / 0 / 0", "bigFont.fnt");
        m_label->setScale(0.4f);
        this->addChild(m_label);
        return true;
    }
    void updateHud() { if (m_label) m_label->setString(fmt::format("{} / {} / {}", cky::currentClicks, cky::highestClicks, cky::totalClicks).c_str()); }
};

namespace cky {
    void ckyBuildHud(PlayLayer* pl) { if (!pl || !pl->m_uiLayer || pl->m_uiLayer->getChildByID("cky-hud")) return; if (auto hud = CkyHud::create()) { hud->setPosition({130.f, 270.f}); pl->m_uiLayer->addChild(hud); } }
    void ckyHudRefresh() { auto pl = PlayLayer::get(); if (!pl || !pl->m_uiLayer) return; if (auto hud = typeinfo_cast<CkyHud*>(pl->m_uiLayer->getChildByID("cky-hud"))) hud->updateHud(); }
    CCNode* ckyHudNode() { auto pl = PlayLayer::get(); if (!pl || !pl->m_uiLayer) return nullptr; return pl->m_uiLayer->getChildByID("cky-hud"); }
}
