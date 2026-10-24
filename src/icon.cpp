#include "state.hpp"
#include <Geode/modify/MenuLayer.hpp>

class $modify(CkyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        
        auto spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        if (!spr) spr = cocos2d::CCSprite::create("GJ_button_01.png");
        
        if (spr) {
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CkyMenuLayer::onClick));
            auto menu = cocos2d::CCMenu::create(btn);
            menu->setPosition({40.f, 80.f});
            this->addChild(menu, 100);
        }
        return true;
    }

    void onClick(cocos2d::CCObject*) {
        cky::ckyOpenPanel(this);
    }
};
