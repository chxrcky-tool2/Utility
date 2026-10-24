#include "state.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>

namespace {
    ccColor3B const kBg = ccc3(150, 90, 55); 
    ccColor3B const kSide = ccc3(110, 60, 35); 
    ccColor3B const kBtn = ccc3(90, 50, 25); 
    ccColor3B const kBlue = ccc3(100, 200, 255);
    enum : int { T_REC = 1, T_PLAY, T_STEPPER, B_TPS_DN, B_TPS_UP, B_SAVE, B_LOAD, L_TPS = 70, TAB_BASE = 100 };
}

class CkyPanel : public cocos2d::CCLayer {
    CCMenu* m_menu = nullptr;
    CCMenu* m_tabMenu = nullptr;
    CCNode* m_content = nullptr;
    cocos2d::CCPoint m_dragStart;
    bool m_isDragging = false;
    int m_currentTab = 0;

public:
    static CkyPanel* create() { 
        auto r = new CkyPanel(); 
        if (r && r->init()) { r->autorelease(); return r; } 
        CC_SAFE_DELETE(r); return nullptr; 
    }
    
    CCMenuItemSpriteExtra* makeBtn(CCMenu* menu, cocos2d::SEL_MenuHandler cb, int tag, float x, float y, float w, float h, char const* text, ccColor3B color) {
        auto bg = cocos2d::extension::CCScale9Sprite::create("square02_001.png"); bg->setContentSize({w, h}); bg->setColor(color);
        auto lbl = cocos2d::CCLabelBMFont::create(text, "bigFont.fnt"); lbl->setScale(0.4f); lbl->setPosition({w/2, h/2}); bg->addChild(lbl);
        auto item = CCMenuItemSpriteExtra::create(bg, this, cb); item->setTag(tag); item->setPosition({x, y}); menu->addChild(item); return item;
    }

    bool init() override {
        if (!cocos2d::CCLayer::init()) return false; 
        this->setID("cky-panel"); 
        float W = 440.f, H = 270.f;
        
        this->setTouchEnabled(true);
        this->setTouchMode(cocos2d::kCCTouchesOneByOne);

        auto frame = cocos2d::extension::CCScale9Sprite::create("GJ_square01.png"); frame->setContentSize({W, H}); frame->setColor(ccc3(255, 105, 180)); this->addChild(frame);
        auto card = cocos2d::extension::CCScale9Sprite::create("GJ_square01.png"); card->setContentSize({W - 10.f, H - 10.f}); card->setColor(kBg); this->addChild(card);
        auto side = cocos2d::extension::CCScale9Sprite::create("GJ_square01.png"); side->setContentSize({118.f, H - 20.f}); side->setColor(kSide); side->setPosition({-149.f, 0.f}); this->addChild(side);
        
        auto title = cocos2d::CCLabelBMFont::create("CHXRCKY", "bigFont.fnt"); title->setScale(0.5f); title->setColor(kBlue); title->setPosition({-149.f, 108.f}); this->addChild(title);
        
        auto closeMenu = cocos2d::CCMenu::create(); this->addChild(closeMenu);
        auto closeBtn = CCMenuItemSpriteExtra::create(cocos2d::CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"), this, menu_selector(CkyPanel::onClose));
        closeBtn->setPosition({W / 2.f - 22.f, H / 2.f - 22.f});
        closeMenu->addChild(closeBtn);

        m_tabMenu = cocos2d::CCMenu::create(); this->addChild(m_tabMenu);
        static char const* names[4] = { "Bot", "Player", "Visual", "Settings" };
        for (int i = 0; i < 4; ++i) {
            makeBtn(m_tabMenu, menu_selector(CkyPanel::onTab), TAB_BASE + i, -149.f, 56.f - 38.f * static_cast<float>(i), 104.f, 30.f, names[i], kSide);
        }

        buildTab(0);
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

    void buildTab(int tab) {
        m_currentTab = tab;
        if (m_content) m_content->removeFromParent();
        m_content = cocos2d::CCNode::create(); this->addChild(m_content);
        
        m_menu = cocos2d::CCMenu::create(); m_content->addChild(m_menu);
        
        static char const* names[4] = { "Bot", "Player", "Visual", "Settings" };
        auto head = cocos2d::CCLabelBMFont::create(names[tab], "bigFont.fnt"); head->setScale(0.5f); head->setColor(kBlue); head->setPosition({80.f, 112.f}); m_content->addChild(head);
        
        float y = 70.f;
        
        if (tab == 0) {
            makeBtn(m_menu, menu_selector(CkyPanel::onToggle), T_REC, 80.f, y, 200.f, 25.f, "RECORD", kBtn); y -= 35.f;
            makeBtn(m_menu, menu_selector(CkyPanel::onToggle), T_PLAY, 80.f, y, 200.f, 25.f, "PLAYBACK", kBtn); y -= 35.f;
            makeBtn(m_menu, menu_selector(CkyPanel::onToggle), T_STEPPER, 80.f, y, 200.f, 25.f, "FRAME STEPPER", kBtn); y -= 35.f;
            makeBtn(m_menu, menu_selector(CkyPanel::onBtn), B_SAVE, 20.f, y, 100.f, 25.f, "SAVE", kBtn);
            makeBtn(m_menu, menu_selector(CkyPanel::onBtn), B_LOAD, 140.f, y, 100.f, 25.f, "LOAD", kBtn); y -= 35.f;
            makeBtn(m_menu, menu_selector(CkyPanel::onBtn), B_TPS_DN, 20.f, y, 30.f, 25.f, "-", kBtn);
            makeBtn(m_menu, menu_selector(CkyPanel::onBtn), B_TPS_UP, 140.f, y, 30.f, 25.f, "+", kBtn);
            auto l = cocos2d::CCLabelBMFont::create("240", "bigFont.fnt"); l->setScale(0.4f); l->setPosition({80.f, y}); l->setTag(L_TPS); m_content->addChild(l);
        } 
        else if (tab == 1) {
            makeBtn(m_menu, menu_selector(CkyPanel::onToggle), 10, 80.f, y, 200.f, 25.f, "NOCLIP", kBtn);
        } 
        else if (tab == 2) {
            makeBtn(m_menu, menu_selector(CkyPanel::onToggle), 11, 80.f, y, 200.f, 25.f, "SHOW HITBOXES", kBtn);
        }
        else if (tab == 3) {
            makeBtn(m_menu, menu_selector(CkyPanel::onBtn), 12, 80.f, y, 200.f, 25.f, "GPL-3.0 LICENSE", kBtn);
        }
    }

    void onTab(cocos2d::CCObject* s) { buildTab(static_cast<cocos2d::CCNode*>(s)->getTag() - TAB_BASE); }
    void onClose(cocos2d::CCObject*) { this->setVisible(false); }
    
    void onToggle(cocos2d::CCObject* s) {
        int t = static_cast<cocos2d::CCNode*>(s)->getTag();
        if (t == T_REC) cky::recording = !cky::recording;
        if (t == T_PLAY) cky::playing = !cky::playing;
        if (t == T_STEPPER) cky::frozen = !cky::frozen;
    }

    void onBtn(cocos2d::CCObject* s) {
        int t = static_cast<cocos2d::CCNode*>(s)->getTag();
        if (t == B_SAVE) cky::save();
        if (t == B_LOAD) cky::load();
    }
};

namespace cky {
    void ckyOpenPanel(CCNode* host) {
        if (!host) return;
        auto p = host->getChildByID("cky-panel");
        if (p) {
            p->setVisible(!p->isVisible());
            return;
        }
        auto newP = CkyPanel::create(); 
        if (newP) {
            auto win = cocos2d::CCDirector::get()->getWinSize();
            newP->setPosition({win.width / 2.f, win.height / 2.f});
            host->addChild(newP, 2000);
        }
    }
    void ckyBuildPanel(PlayLayer* pl) { ckyOpenPanel(pl->m_uiLayer); }
}
