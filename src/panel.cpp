#include "state.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>

class CkyPanel : public cocos2d::CCNode {
public:
    static CkyPanel* create() {
        auto ret = new CkyPanel();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret); return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-panel");
        this->setPosition({40.f, 160.f}); 

        auto bg = cocos2d::extension::CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({180.f, 260.f});
        bg->setOpacity(220);
        this->addChild(bg);

        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({60.f, 130.f}); 
        this->addChild(menu);

        auto mkToggle = [&](const char* text, bool* stateVar, float yPos) {
            auto toggler = CCMenuItemToggler::createWithStandardSprites(
                this, // We will fix the cast here
                menu_selector(CkyPanel::onToggle), 
                0.6f
            );
            toggler->setPosition({0.f, yPos}); 
            toggler->setTag(reinterpret_cast<intptr_t>(stateVar)); 
            toggler->toggle(*stateVar); 
            menu->addChild(toggler);
            
            auto label = cocos2d::CCLabelBMFont::create(text, "bigFont.fnt");
            label->setScale(0.35f);
            label->setAnchorPoint({0.f, 0.5f});
            label->setPosition({80.f, 130.f + yPos}); 
            this->addChild(label);
        };

        mkToggle("Noclip", &cky::noclip, 90.f);
        mkToggle("Record", &cky::recording, 45.f);
        mkToggle("Play", &cky::playing, 0.f);
        mkToggle("Auto Clicker", &cky::autoClicker, -45.f);
        mkToggle("Clickbot", &cky::clickbotSounds, -90.f);

        return true;
    }

    void onToggle(cocos2d::CCObject* sender) {
        auto toggler = geode::cast::typeinfo_cast<CCMenuItemToggler*>(sender);
        if (toggler) {
            bool* stateVar = reinterpret_cast<bool*>(toggler->getTag());
            if (stateVar) *stateVar = !(*stateVar);
        }
    }
};

void ckyBuildPanel(PlayLayer* pl) {
    if (!pl || !pl->m_uiLayer) return;
    if (pl->m_uiLayer->getChildByID("cky-panel")) return;
    if (auto panel = CkyPanel::create()) {
        pl->m_uiLayer->addChild(panel, 100); 
    }
}
