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
        
        auto spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_roundButton_001.png");
        if (!spr) spr = cocos2d::CCSprite::create("GJ_button_01.png");
        if (!spr) return false;

        spr->setScale(1.0f);
        
        // Create the button
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CkyIcon::onClick));
        if (!btn) return false;

        // Create the menu FIRST without items, then add the button
        auto menu = cocos2d::CCMenu::create();
        if (menu) {
            menu->setPosition({0.f, 0.f});
            menu->addChild(btn); // Add the button after menu creation
            this->addChild(menu);
        }
        
        return true;
    }

    void onClick(cocos2d::CCObject*) {
        geode::log::info("Cky Icon Clicked!");
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
