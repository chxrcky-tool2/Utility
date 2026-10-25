#include "state.hpp"
#include <Geode/binding/CCMenuItemToggler.hpp>

class CkyPanel : public cocos2d::CCLayer {
public:
    static CkyPanel* create() { 
        auto r = new CkyPanel(); 
        if (r && r->init()) { r->autorelease(); return r; } 
        CC_SAFE_DELETE(r); return nullptr; 
    }
    
    bool init() override {
        if (!cocos2d::CCLayer::init()) return false; 
        this->setID("cky-panel"); 
        this->setScale(0.7f);
        
        auto bg = cocos2d::extension::CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({440.f, 270.f});
        bg->setColor({150, 90, 55});
        this->addChild(bg);

        auto closeBtn = CCMenuItemSpriteExtra::create(cocos2d::CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"), this, menu_selector(CkyPanel::onClose));
        auto closeMenu = cocos2d::CCMenu::create(closeBtn);
        closeBtn->setPosition({200.f, 115.f});
        this->addChild(closeMenu);

        return true;
    }

    void onClose(cocos2d::CCObject*) { 
        this->setVisible(false); 
    }
};

// Define all the missing functions the linker is screaming about here
namespace cky {
    void ckyOpenPanel(CCNode* host) {
        if (!host) return;
        auto p = host->getChildByID("cky-panel");
        if (p) {
            p->setVisible(!p->isVisible());
            return;
        }
        auto newP = CkyPanel::create(); 
        if (newP) {
            auto win = cocos2d::CCDirector::get()->getWinSize();
            newP->setPosition({win.width / 2.f, win.height / 2.f});
            host->addChild(newP, 2000);
        }
    }
    
    void ckyBuildPanel(PlayLayer* pl) { ckyOpenPanel(pl->m_uiLayer); }
    void ckyBuildHud(PlayLayer* pl) {}
    void ckyBuildStartPos(PlayLayer* pl) {}
}
