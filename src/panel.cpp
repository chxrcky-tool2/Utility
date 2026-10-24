#include "state.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>

// Changed from CCNode to CCLayer
class CkyPanel : public cocos2d::CCLayer {
public:
    cocos2d::CCPoint m_originalPos;

    static CkyPanel* create() {
        auto ret = new CkyPanel();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret); return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        
        this->setID("cky-panel");

        auto savedX = geode::Mod::get()->getSavedValue<float>("panel_x", 40.f);
        auto savedY = geode::Mod::get()->getSavedValue<float>("panel_y", 160.f);
        this->setPosition({savedX, savedY}); 

        // CCLayer HAS this function!
        this->setTouchEnabled(true);
        this->setTouchMode(cocos2d::kCCTouchesOneByOne);

        auto bg = cocos2d::extension::CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({180.f, 260.f});
        bg->setOpacity(220);
        this->addChild(bg);

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

    bool onTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        auto touchPos = this->convertToNodeSpace(touch->getLocation());
        auto size = cocos2d::CCSize(180.f, 260.f);
        if (touchPos.x > 0 && touchPos.x < size.width && touchPos.y > 0 && touchPos.y < size.height) {
            m_originalPos = this->getPosition() - touch->getLocation();
            return true; 
        }
        return false;
    }

    void onTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        this->setPosition(touch->getLocation() + m_originalPos);
    }

    void onTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        geode::Mod::get()->setSavedValue<float>("panel_x", this->getPositionX());
        geode::Mod::get()->setSavedValue<float>("panel_y", this->getPositionY());
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
