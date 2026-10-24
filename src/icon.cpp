#include "state.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

class CkyFloatingButton : public cocos2d::CCLayer {
    cocos2d::CCPoint m_dragStart;
    bool m_isDragging = false;
    cocos2d::CCSprite* m_sprite = nullptr;

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

        m_sprite = cocos2d::CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        if (!m_sprite) m_sprite = cocos2d::CCSprite::create("GJ_button_01.png");
        
        if (m_sprite) {
            m_sprite->setScale(1.1f);
            this->addChild(m_sprite); 
        }
        
        return true;
    }

    bool onTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
        auto touchPos = this->getParent()->convertToNodeSpace(touch->getLocation());
        if (this->boundingBox().containsPoint(touchPos)) {
            m_dragStart = touchPos - this->getPosition();
            m_isDragging = false; 
            return true;
        }
        return false;
    }

    void onTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
        auto touchPos = this->getParent()->convertToNodeSpace(touch->getLocation());
        auto diff = touchPos - this->getPosition();
        
        if (std::abs(diff.x) > 5.f || std::abs(diff.y) > 5.f) {
            m_isDragging = true;
        }
        
        if (m_isDragging) {
            this->setPosition(touchPos - m_dragStart);
        }
    }

    void onTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
        if (!m_isDragging) {
            auto parent = this->getParent();
            if (parent) cky::ckyOpenPanel(parent);
        }
        m_isDragging = false;
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
