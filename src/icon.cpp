#include "state.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Amber.hpp>

using namespace amber;

class $modify(CkyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        this->scheduleOnce(schedule_selector(CkyMenuLayer::addFloatingButton), 0.5f);
        return true;
    }
    
    void addFloatingButton(float) {
        auto spr = CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        if (!spr) return;
        
        auto btn = DraggableButton::create(spr, [](CCObject* sender) {
            auto parent = static_cast<CCNode*>(sender)->getParent();
            if (parent) cky::ckyOpenPanel(parent);
        });
        
        if (btn) {
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
        auto spr = CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        if (!spr) return;
        
        auto btn = DraggableButton::create(spr, [](CCObject* sender) {
            auto parent = static_cast<CCNode*>(sender)->getParent();
            if (parent) cky::ckyOpenPanel(parent);
        });
        
        if (btn) {
            auto winSize = CCDirector::get()->getWinSize();
            btn->setPosition({winSize.width - 40.f, winSize.height - 80.f});
            this->addChild(btn, 100);
        }
    }
};
