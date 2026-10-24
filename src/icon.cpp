#include "state.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

class CkyFloatingButton : public cocos2d::CCLayer {
    cocos2d::CCPoint m_dragStart;
    bool m_isDragging = false;

public:
    static CkyFloatingButton* create() {
        auto ret = new CkyFloatingButton();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret); return nullptr;
    }

    bool init() override {
        if (!cocos2d::CCLayer::init()) return false;
        this->setID("cky-floating-btn");
        
        this->setTouchEnabled(true);
        this->setTouchMode(cocos2d::kCCTouchesOneByOne);

        auto spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        if (!spr) spr = cocos2d::CCSprite::create("GJ_button_01.png");
        if (spr) {
            spr->setScale(1.1f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CkyFloatingButton::onClick));
            auto menu = cocos2d::CCMenu::create(btn);
            menu->setPosition({0.f, 0.f});
            this->addChild(menu);
        }
        
        return true;
    }

    bool onTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
        auto touchPos = this->getParent()->convertToNodeSpace(touch->getLocation());
        if (this->boundingBox().containsPoint(touchPos)) {
            m_dragStart = touchPos - this->getPosition();
            m_isDragging = true;
            return true;
        }
        return false;
    }

    void onTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
        if (m_isDragging) {
            auto touchPos = this->getParent()->convertToNodeSpace(touch->getLocation());
            this->setPosition(touchPos - m_dragStart);
        }
    }

    void onTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
        m_isDragging = false;
    }

    void onClick(cocos2d::CCObject*) {
        auto parent = this->getParent();
        if (parent) cky::ckyOpenPanel(parent);
    }
};

class $modify(CkyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        this->scheduleOnce(schedule_selector(CkyMenuLayer::addFloatingButton), 0.3f);
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
        this->scheduleOnce(schedule_selector(CkyPauseLayer::addFloatingButton), 0.3f);
    }
    
    void addFloatingButton(float) {
        if (auto btn = CkyFloatingButton::create()) {
            auto winSize = cocos2d::CCDirector::get()->getWinSize();
            btn->setPosition({winSize.width - 40.f, winSize.height - 80.f});
            this->addChild(btn, 100);
        }
    }
};
