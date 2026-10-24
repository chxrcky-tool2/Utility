#include "state.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

class CkyIcon : public cocos2d::CCNode {
public:
    static CkyIcon* create() { auto ret = new CkyIcon(); if (ret && ret->init()) { ret->autorelease(); return ret; } CC_SAFE_DELETE(ret); return nullptr; }
    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-icon");
        auto spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_commentIcon_001.png");
        spr->setScale(1.2f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CkyIcon::onClick));
        auto menu = cocos2d::CCMenu::create(btn);
        this->addChild(menu);
        return true;
    }
    void onClick(cocos2d::CCObject*) { geode::log::info("Icon Clicked!"); }
};

class $modify(CkyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        if (auto icon = CkyIcon::create()) { icon->setPosition({40.f, 40.f}); this->addChild(icon, 100); }
        return true;
    }
};

class $modify(CkyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        if (auto icon = CkyIcon::create()) { auto winSize = cocos2d::CCDirector::get()->getWinSize(); icon->setPosition({winSize.width - 40.f, winSize.height - 40.f}); this->addChild(icon, 100); }
    }
};
