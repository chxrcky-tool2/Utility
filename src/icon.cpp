#include "state.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

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
        
        // Use the Edit button (pencil icon) so it never glitches
        auto spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        if (!spr) spr = cocos2d::CCSprite::create("GJ_button_01.png");
        spr->setScale(1.1f);

        // Create the button but add it to a menu so it can be clicked
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CkyFloatingButton::onClick));
        auto menu = cocos2d::CCMenu::create(btn);
        menu->setPosition({0.f, 0.f});
        this->addChild(menu);

        // Make the button draggable
        auto listener = cocos2d::CCEventListenerTouchOneByOne::create();
        listener->setSwallowTouches(true);
        
        listener->onTouchBegan = [this](cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
            auto touchPos = this->getParent()->convertToNodeSpace(touch->getLocation());
            if (this->boundingBox().containsPoint(touchPos)) {
                m_dragStart = touchPos - this->getPosition();
                m_isDragging = true;
                return true;
            }
            return false;
        };
        
        listener->onTouchMoved = [this](cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
            if (m_isDragging) {
                auto touchPos = this->getParent()->convertToNodeSpace(touch->getLocation());
                this->setPosition(touchPos - m_dragStart);
            }
        };
        
        listener->onTouchEnded = [this](cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
            m_isDragging = false;
        };
        
        // Priority 0 means it handles touches normally
        cocos2d::CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(listener, 0, true);
        listener->autorelease();
        
        return true;
    }

    void onClick(cocos2d::CCObject*) {
        // Toggle the panel visibility
        auto parent = this->getParent();
        if (parent) {
            cky::ckyOpenPanel(parent);
        }
    }
};

// Add to Main Menu
class $modify(CkyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        
        // Delay to ensure the scene is fully loaded
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

// Add to Pause Menu
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
