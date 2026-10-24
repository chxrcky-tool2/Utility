#include "state.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/cocos/include/CCTouchDispatcher.h>
#include <Geode/cocos/layers_scenes_transitions_nodes/CCLayer.h>

class CkyPanel : public cocos2d::CCNode {
public:
    cocos2d::CCPoint m_originalPos;

    static CkyPanel* create() {
        auto ret = new CkyPanel();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret); return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        
        // 1. NODE ID: This ensures we never spawn duplicates
        this->setID("cky-panel");

        // 2. MEMORY: Load saved position or default to left side
        auto savedX = geode::Mod::get()->getSavedValue<float>("panel_x", 40.f);
        auto savedY = geode::Mod::get()->getSavedValue<float>("panel_y", 160.f);
        this->setPosition({savedX, savedY}); 

        // 3. DRAGGABLE: The GDH-style touch listener
        auto dragListener = cocos2d::CCEventListenerTouchOneByOne::create();
        dragListener->setSwallowTouches(true);
        
        dragListener->onTouchBegan = [this](cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
            auto touchPos = this->convertToNodeSpace(touch->getLocation());
            auto size = cocos2d::CCSize(180.f, 260.f);
            if (touchPos.x > 0 && touchPos.x < size.width && touchPos.y > 0 && touchPos.y < size.height) {
                m_originalPos = this->getPosition() - touch->getLocation();
                return true;
            }
            return false;
        };
        
        dragListener->onTouchMoved = [this](cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
            this->setPosition(touch->getLocation() + m_originalPos);
        };
        
        dragListener->onTouchEnded = [this](cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
            // Save the new position!
            geode::Mod::get()->setSavedValue<float>("panel_x", this->getPositionX());
            geode::Mod::get()->setSavedValue<float>("panel_y", this->getPositionY());
        };
        
        cocos2d::CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(dragListener, 0, true);
        dragListener->autorelease();

        // Background
        auto bg = cocos2d::extension::CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({180.f, 260.f});
        bg->setOpacity(220);
        this->addChild(bg);

        // Menu (Only holds toggles)
        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({60.f, 130.f}); 
        this->addChild(menu);

        auto mkToggle = [&](const char* text, bool* stateVar, float yPos) {
            auto toggler = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(CkyPanel::onToggle), 0.6f);
            toggler->setPosition({0.f, yPos}); 
            toggler->setTag(reinterpret_cast<intptr_t>(stateVar)); 
            toggler->toggle(*stateVar); 
            menu->addChild(toggler);
            
            auto label = cocos2d::CCLabelBMFont::create(text, "bigFont.fnt");
            label->setScale(0.35f);
            label->setAnchorPoint({0.f, 0.5f});
            label->setPosition({80.f, 130.f + yPos}); 
            this->addChild(label);
        };

        mkToggle("Noclip", &cky::noclip, 90.f);
        mkToggle("Record", &cky::recording, 45.f);
        mkToggle("Play", &cky::playing, 0.f);
        mkToggle("Auto Clicker", &cky::autoClicker, -45.f);
        mkToggle("Clickbot", &cky::clickbotSounds, -90.f);

        return true;
    }

    void onToggle(cocos2d::CCObject* sender) {
        auto toggler = geode::cast::typeinfo_cast<CCMenuItemToggler*>(sender);
        if (toggler) {
            bool* stateVar = reinterpret_cast<bool*>(toggler->getTag());
            if (stateVar) *stateVar = !(*stateVar);
        }
    }
};

void ckyBuildPanel(PlayLayer* pl) {
    if (!pl || !pl->m_uiLayer) return;
    
    // NODE ID CHECK: This is the magic line. It prevents the crash.
    if (pl->m_uiLayer->getChildByID("cky-panel")) return;
    
    if (auto panel = CkyPanel::create()) {
        // Z-Order 100 puts it above the game but below pause menus
        pl->m_uiLayer->addChild(panel, 100); 
    }
}
