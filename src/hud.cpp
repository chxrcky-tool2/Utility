#include "state.hpp"

class CkyHud : public cocos2d::CCNode {
public:
    cocos2d::CCLabelBMFont* m_label = nullptr;

    static CkyHud* create() {
        auto ret = new CkyHud();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret); return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-hud");
        this->setPosition({130.f, 270.f});

        m_label = cocos2d::CCLabelBMFont::create("0 / 0 / 0", "bigFont.fnt");
        m_label->setScale(0.4f);
        m_label->setAnchorPoint({0.5f, 0.5f});
        this->addChild(m_label);
        return true;
    }

    void updateHud() {
        if (m_label) {
            std::string text = fmt::format("{} / {} / {}", cky::currentClicks, cky::highestClicks, cky::totalClicks);
            m_label->setString(text.c_str());
        }
    }
};

void ckyBuildHud(PlayLayer* pl) {
    if (!pl || !pl->m_uiLayer) return;
    if (pl->m_uiLayer->getChildByID("cky-hud")) return;
    if (auto hud = CkyHud::create()) pl->m_uiLayer->addChild(hud, 10);
}

void ckyHudRefresh() {
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_uiLayer) return;
    if (auto hud = typeinfo_cast<CkyHud*>(pl->m_uiLayer->getChildByID("cky-hud"))) {
        hud->updateHud();
    }
}

CCNode* ckyHudNode() {
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_uiLayer) return nullptr;
    return pl->m_uiLayer->getChildByID("cky-hud");
}
