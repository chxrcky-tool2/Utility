#include "state.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

class CkyIcon : public cocos2d::CCNode {
public:
    static CkyIcon* create() {
        auto ret = new CkyIcon();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret); return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-icon");
        
        // Use the clean Edit button so it doesn't glitch
        auto spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        if (!spr) spr = cocos2d::CCSprite::create("GJ_button_01.png");
        
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CkyIcon::onClick));
        auto menu = cocos2d::CCMenu::create();
        menu->addChild(btn);
        this->addChild(menu);
        return true;
    }

    void onClick(cocos2d::CCObject*) {
        // Toggle the panel visibility
        auto parentLayer = this->getParent();
        if (!parentLayer) return;
        
        auto panel = parentLayer->getChildByID("cky-panel");
        if (panel) {
            panel->removeFromParent(); // If it's open, close it
        } else {
            cky::ckyOpenPanel(parentLayer); // If it's closed, open it
        }
    }
};

class $modify(CkyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        this->scheduleOnce(schedule_selector(CkyMenuLayer::addCkyIcon), 0.5f);
        return true;
    }
    void addCkyIcon(float) {
        if (auto icon = CkyIcon::create()) {
            icon->setPosition({40.f, 40.f});
            this->addChild(icon, 100);
        }
    }
};

class $modify(CkyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        this->scheduleOnce(schedule_selector(CkyPauseLayer::addCkyPauseIcon), 0.5f);
    }
    void addCkyPauseIcon(float) {
        if (auto icon = CkyIcon::create()) {
            auto winSize = cocos2d::CCDirector::get()->getWinSize();
            icon->setPosition({winSize.width - 40.f, winSize.height - 40.f});
            this->addChild(icon, 100);
        }
    }
};
