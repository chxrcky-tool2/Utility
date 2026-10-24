#include "state.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

class CkyFloatingButton : public cocos2d::CCNode {
    cocos2d::CCPoint m_dragStart;
    bool m_isDragging = false;

public:
    static CkyFloatingButton* create() {
        auto ret = new CkyFloatingButton();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret); return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-floating-btn");

        auto spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        if (!spr) spr = cocos2d::CCSprite::create("GJ_button_01.png");
        if (spr) {
            spr->setScale(1.1f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CkyFloatingButton::onClick));
            auto menu = cocos2d::CCMenu::create();
            menu->addChild(btn);
            this->addChild(menu);
        }
        return true;
    }

    void onClick(cocos2d::CCObject*) {
        auto parent = this->getParent();
        if (parent) cky::ckyOpenPanel(parent);
    }
};

class $modify(CkyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        this->scheduleOnce(schedule_selector(CkyMenuLayer::addFloatingButton), 0.5f);
        return true;
    }
    
    void addFloatingButton(float) {
        if (auto btn = CkyFloatingButton::create()) {
            btn->setPosition({40.f, 80.f});
            this->addChild(btn, 100);
        }
    }
};

class $modify(CkyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        this->scheduleOnce(schedule_selector(CkyPauseLayer::addFloatingButton), 0.5f);
    }
    
    void addFloatingButton(float) {
        if (auto btn = CkyFloatingButton::create()) {
            auto winSize = cocos2d::CCDirector::get()->getWinSize();
            btn->setPosition({winSize.width - 40.f, winSize.height - 80.f});
            this->addChild(btn, 100);
        }
    }
};
