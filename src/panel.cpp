#include "state.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/ui/Popup.hpp>

class CkyPanel : public geode::Popup<> {
public:
    static CkyPanel* create() {
        auto ret = new CkyPanel();
        if (ret && ret->init(240.f, 280.f)) { 
            ret->autorelease(); 
            return ret; 
        }
        CC_SAFE_DELETE(ret); 
        return nullptr;
    }

    bool setup() override {
        this->setID("cky-panel");
        this->setTitle("ChxrCky");

        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({120.f, 140.f}); 
        this->m_mainLayer->addChild(menu);

        auto mkToggle = [&](const char* text, bool* stateVar, float yPos) {
            auto toggler = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(CkyPanel::onToggle), 0.7f);
            toggler->setPosition({-70.f, yPos}); 
            toggler->setTag(reinterpret_cast<intptr_t>(stateVar)); 
            toggler->toggle(*stateVar); 
            menu->addChild(toggler);
            
            auto label = cocos2d::CCLabelBMFont::create(text, "bigFont.fnt");
            label->setScale(0.45f);
            label->setAnchorPoint({0.f, 0.5f});
            label->setPosition({-40.f, yPos}); 
            menu->addChild(label);
        };

        mkToggle("Noclip", &cky::noclip, 80.f);
        mkToggle("Record", &cky::recording, 40.f);
        mkToggle("Play", &cky::playing, 0.f);
        mkToggle("Auto Clicker", &cky::autoClicker, -40.f);
        mkToggle("Clickbot", &cky::clickbotSounds, -80.f);

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
    // If the panel is already open, don't open another one
    if (pl->m_uiLayer->getChildByID("cky-panel")) return;
    
    if (auto panel = CkyPanel::create()) {
        panel->show(); // This is Geode's built-in popup animator!
    }
}
