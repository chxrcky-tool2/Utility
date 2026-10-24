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
        
        // Fix: Safely load the sprite frame
        auto frame = cocos2d::CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName("GJ_commentIcon_001.png");
        if (!frame) frame = cocos2d::CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName("GJ_chatBtn_001.png");
        
        auto spr = cocos2d::CCSprite::createWithSpriteFrame(frame);
        if (!spr) spr = cocos2d::CCSprite::create("GJ_commentIcon_001.png");
        if (spr) spr->setScale(1.2f);
        
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CkyIcon::onClick));
        if (!btn) return false;

        auto menu = cocos2d::CCMenu::create(btn);
        if (menu) {
            menu->setPosition({0.f, 0.f});
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
        
        // Fix: Delay the icon creation by one frame so sprites load
        this->scheduleOnce(schedule_selector(CkyMenuLayer::addCkyIcon), 0.1f);
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
        this->scheduleOnce(schedule_selector(CkyPauseLayer::addCkyPauseIcon), 0.1f);
    }

    void addCkyPauseIcon(float) {
        if (auto icon = CkyIcon::create()) {
            auto winSize = cocos2d::CCDirector::get()->getWinSize();
            icon->setPosition({winSize.width - 40.f, winSize.height - 40.f});
            this->addChild(icon, 100);
        }
    }
};
