#include "state.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/utils/cocos.hpp>

class CkyPanel : public cocos2d::CCNode {
public:
    static CkyPanel* create() {
        auto ret = new CkyPanel();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret); return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-panel");
        // Default position (left side)
        this->setPosition({60.f, 160.f}); 

        // The background box
        auto bg = cocos2d::extension::CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({180.f, 260.f});
        bg->setOpacity(220);
        this->addChild(bg);

        // The menu
        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f}); 
        this->addChild(menu);

        auto mkToggle = [&](const char* text, bool* stateVar, float yPos) {
            // Place toggle on the left
            auto toggler = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(CkyPanel::onToggle), 0.6f);
            toggler->setPosition({30.f, 130.f + yPos}); 
            toggler->setTag(reinterpret_cast<intptr_t>(stateVar)); 
            toggler->toggle(*stateVar); 
            menu->addChild(toggler);
            
            // Place text on the right
            auto label = cocos2d::CCLabelBMFont::create(text, "bigFont.fnt");
            label->setScale(0.35f);
            label->setAnchorPoint({0.f, 0.5f});
            label->setPosition({50.f, 130.f + yPos}); 
            this->addChild(label);
        };

        mkToggle("Noclip", &cky::noclip, 90.f);
        mkToggle("Record", &cky::recording, 45.f);
        mkToggle("Play", &cky::playing, 0.f);
        mkToggle("Auto Clicker", &cky::autoClicker, -45.f);
        mkToggle("Clickbot", &cky::clickbotSounds, -90.f);

        // Dragging logic using Geode's built-in touch system
        auto listener = cocos2d::CCEventListenerTouchOneByOne::create();
        listener->setSwallowTouches(true);
        
        listener->onTouchBegan = [this](cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
            auto touchPos = this->convertToNodeSpace(touch->getLocation());
            auto size = cocos2d::CCSize(180.f, 260.f);
            if (touchPos.x > 0 && touchPos.x < size.width && touchPos.y > 0 && touchPos.y < size.height) {
                return true;
            }
            return false;
        };
        
        listener->onTouchMoved = [this](cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
            this->setPosition(this->getPosition() + touch->getDelta());
        };
        
        cocos2d::CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(listener, 0, true);
        listener->autorelease();

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
    if (pl->m_uiLayer->getChildByID("cky-panel")) return;
    if (auto panel = CkyPanel::create()) {
        pl->m_uiLayer->addChild(panel, 100); 
    }
}
