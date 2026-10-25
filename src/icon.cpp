#include "state.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

class $modify(CkyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        this->scheduleOnce(schedule_selector(CkyMenuLayer::addBtn), 0.5f);
        return true;
    }
    void addBtn(float) {
        auto spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        if (spr) {
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CkyMenuLayer::onClick));
            auto menu = cocos2d::CCMenu::create(btn);
            menu->setPosition({40.f, 80.f});
            this->addChild(menu, 100);
        }
    }
    void onClick(cocos2d::CCObject*) { cky::ckyOpenPanel(this); }
};

class $modify(CkyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        this->scheduleOnce(schedule_selector(CkyPauseLayer::addBtn), 0.5f);
    }
    void addBtn(float) {
        auto spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        if (spr) {
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CkyPauseLayer::onClick));
            auto menu = cocos2d::CCMenu::create(btn);
            auto winSize = cocos2d::CCDirector::get()->getWinSize();
            menu->setPosition({winSize.width - 40.f, winSize.height - 80.f});
            this->addChild(menu, 100);
        }
    }
    void onClick(cocos2d::CCObject*) { cky::ckyOpenPanel(this); }
};
