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
        // Moved position to align nicely inside the box
        this->setPosition({50.f, 280.f});

        m_label = cocos2d::CCLabelBMFont::create("0 / 0 / 0", "bigFont.fnt");
        m_label->setScale(0.4f);
        // Changed anchor point so it centers the text
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
