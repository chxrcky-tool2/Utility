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

        auto scroll = cocos2d::extension::CCScrollView::create();
        scroll->setContentSize({180.f, 200.f});
        scroll->setPosition({90.f, 130.f}); 
        scroll->setDirection(cocos2d::extension::kCCScrollViewDirectionVertical);
        this->addChild(scroll);

        auto content = cocos2d::CCNode::create();
        content->setContentSize({180.f, 400.f});
        scroll->setContainer(content);

        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({90.f, 380.f});
        content->addChild(menu);

        auto mkToggle = [&](const char* text, bool* stateVar, float yPos) {
            auto label = cocos2d::CCLabelBMFont::create(text, "bigFont.fnt");
            label->setScale(0.35f);
            
            auto toggler = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(CkyPanel::onToggle), 0.6f);
            toggler->setPosition({-40.f, yPos});
            toggler->setTag(reinterpret_cast<intptr_t>(stateVar)); 
            toggler->toggle(*stateVar); 
            menu->addChild(toggler);
            
            label->setPosition({10.f, yPos});
            menu->addChild(label);
        };

        mkToggle("Noclip", &cky::noclip, 0.f);
        mkToggle("Record", &cky::recording, -40.f);
        mkToggle("Play", &cky::playing, -80.f);
        mkToggle("Auto Clicker", &cky::autoClicker, -120.f);
        mkToggle("Clickbot", &cky::clickbotSounds, -160.f);

        auto licenseBtn = cocos2d::CCMenuItemLabel::create(
            cocos2d::CCLabelBMFont::create("GPL-3.0 License", "bigFont.fnt"),
            this, menu_selector(CkyPanel::onLicense)
        );
        licenseBtn->setScale(0.3f);
        licenseBtn->setOpacity(153);
        licenseBtn->setPosition({0.f, -240.f});
        menu->addChild(licenseBtn);

        return true;
    }

    void onToggle(cocos2d::CCObject* sender) {
        auto toggler = typeinfo_cast<CCMenuItemToggler*>(sender);
        if (toggler) {
            bool* stateVar = reinterpret_cast<bool*>(toggler->getTag());
            if (stateVar) *stateVar = !(*stateVar);
        }
    }

    void onLicense(cocos2d::CCObject*) {
        geode::createQuickPopup("License", "Licensed under GPL-3.0.\nYou cannot make this closed source.", "OK", nullptr, nullptr);
    }
};

void ckyBuildPanel(PlayLayer* pl) {
    if (!pl || !pl->m_uiLayer) return;
    if (pl->m_uiLayer->getChildByID("cky-panel")) return;
    if (auto panel = CkyPanel::create()) pl->m_uiLayer->addChild(panel, 20);
}
