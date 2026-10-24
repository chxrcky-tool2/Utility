#include "state.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>

namespace {
    ccColor3B const kBg = ccc3(150, 90, 55); ccColor3B const kSide = ccc3(110, 60, 35); ccColor3B const kBtn = ccc3(90, 50, 25); ccColor3B const kTxt = ccc3(255, 255, 255); ccColor3B const kBlue = ccc3(100, 200, 255);
    enum : int { T_REC = 1, T_PLAY, T_STEPPER, B_TPS_DN, B_TPS_UP, B_SPD_DN, B_SPD_UP, B_SAVE, B_LOAD, L_TPS = 70, L_SPEED, L_INFO, TAB_BASE = 100 };
}

class CkyPanel : public CCNode {
    CCNode* m_content = nullptr; CCMenu* m_menu = nullptr; CCMenu* m_tabMenu = nullptr;
    float m_y = 0.f; int m_tab = 0;
public:
    static CkyPanel* create() { auto r = new CkyPanel(); if (r && r->init()) { r->autorelease(); return r; } CC_SAFE_DELETE(r); return nullptr; }
    
    void attachTo(CCNode* host) {
        auto win = CCDirector::get()->getWinSize();
        this->setPosition({win.width / 2.f, win.height / 2.f}); 
        host->addChild(this, 2000);
    }

    CCMenuItemSpriteExtra* makeBtn(CCMenu* menu, SEL_MenuHandler cb, int tag, float x, float y, float w, float h, char const* text, ccColor3B color) {
        auto bg = CCScale9Sprite::create("square02_001.png"); bg->setContentSize({w, h}); bg->setColor(color);
        auto lbl = CCLabelBMFont::create(text, "bigFont.fnt"); lbl->setScale(0.4f); lbl->setPosition({w/2, h/2}); bg->addChild(lbl);
        auto item = CCMenuItemSpriteExtra::create(bg, this, cb); item->setTag(tag); item->setPosition({x, y}); menu->addChild(item); return item;
    }

    bool init() override {
        if (!CCNode::init()) return false; this->setID("cky-panel"); float W = 440.f, H = 270.f;
        
        auto frame = CCScale9Sprite::create("GJ_square01.png"); frame->setContentSize({W, H}); frame->setColor(ccc3(255, 105, 180)); this->addChild(frame);
        auto card = CCScale9Sprite::create("GJ_square01.png"); card->setContentSize({W - 10.f, H - 10.f}); card->setColor(kBg); this->addChild(card);
        auto side = CCScale9Sprite::create("GJ_square01.png"); side->setContentSize({118.f, H - 20.f}); side->setColor(kSide); side->setPosition({-149.f, 0.f}); this->addChild(side);
        
        auto title = CCLabelBMFont::create("CHXRCKY", "bigFont.fnt"); title->setScale(0.5f); title->setColor(kBlue); title->setPosition({-149.f, 108.f}); this->addChild(title);
        
        m_tabMenu = CCMenu::create(); m_tabMenu->setPosition({0.f, 0.f}); this->addChild(m_tabMenu);
        
        // REMOVED "Converter" and "Render"
        static char const* names[4] = { "Bot", "Player", "Visual", "Settings" };
        for (int i = 0; i < 4; ++i) makeBtn(m_tabMenu, menu_selector(CkyPanel::onTab), TAB_BASE + i, -149.f, 56.f - 38.f * static_cast<float>(i), 104.f, 30.f, names[i], kSide);

        auto closeMenu = CCMenu::create(); closeMenu->setPosition({0.f, 0.f});
        auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"); closeSpr->setScale(0.55f);
        auto closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(CkyPanel::onClose)); closeBtn->setPosition({W / 2.f - 22.f, H / 2.f - 22.f}); closeMenu->addChild(closeBtn); this->addChild(closeMenu);
        
        buildTab(0); return true;
    }

    void buildTab(int tab) {
        m_tab = tab; if (m_content) m_content->removeFromParent(); m_content = CCNode::create(); this->addChild(m_content);
        m_menu = CCMenu::create(); m_menu->setPosition({0.f, 0.f}); m_content->addChild(m_menu);
        
        // REMOVED "Converter" and "Render"
        static char const* names[4] = { "Bot", "Player", "Visual", "Settings" };
        auto head = CCLabelBMFont::create(names[tab], "bigFont.fnt"); head->setScale(0.5f); head->setColor(kBlue); head->setPosition({80.f, 112.f}); m_content->addChild(head);
        m_y = 70.f;
        
        if (tab == 0) { // Bot
            makeBtn(m_menu, menu_selector(CkyPanel::onToggle), T_REC, 80.f, m_y, 200.f, 25.f, "RECORD", kBtn); m_y -= 35.f;
            makeBtn(m_menu, menu_selector(CkyPanel::onToggle), T_PLAY, 80.f, m_y, 200.f, 25.f, "PLAYBACK", kBtn); m_y -= 35.f;
            makeBtn(m_menu, menu_selector(CkyPanel::onToggle), T_STEPPER, 80.f, m_y, 200.f, 25.f, "FRAME STEPPER", kBtn); m_y -= 35.f;
            makeBtn(m_menu, menu_selector(CkyPanel::onBtn), B_SAVE, 20.f, m_y, 100.f, 25.f, "SAVE", kBtn);
            makeBtn(m_menu, menu_selector(CkyPanel::onBtn), B_LOAD, 140.f, m_y, 100.f, 25.f, "LOAD", kBtn); m_y -= 35.f;
            makeBtn(m_menu, menu_selector(CkyPanel::onBtn), B_TPS_DN, 20.f, m_y, 30.f, 25.f, "-", kBtn);
            makeBtn(m_menu, menu_selector(CkyPanel::onBtn), B_TPS_UP, 140.f, m_y, 30.f, 25.f, "+", kBtn);
            auto l = CCLabelBMFont::create("240", "bigFont.fnt"); l->setScale(0.4f); l->setPosition({80.f, m_y}); l->setTag(L_TPS); m_content->addChild(l);
        } 
        else if (tab == 1) { // Player
            makeBtn(m_menu, menu_selector(CkyPanel::onToggle), T_REC, 80.f, m_y, 200.f, 25.f, "NOCLIP", kBtn);
        } 
        else if (tab == 2) { // Visual
            makeBtn(m_menu, menu_selector(CkyPanel::onToggle), T_REC, 80.f, m_y, 200.f, 25.f, "SHOW HITBOXES", kBtn);
        }
        else if (tab == 3) { // Settings
            makeBtn(m_menu, menu_selector(CkyPanel::onBtn), B_SAVE, 80.f, m_y, 200.f, 25.f, "GPL-3.0 LICENSE", kBtn);
        }
    }

    void onTab(CCObject* s) { buildTab(static_cast<CCNode*>(s)->getTag() - TAB_BASE); }
    void onClose(CCObject*) { this->removeFromParent(); }
    
    void onToggle(CCObject* s) {
        int t = static_cast<CCNode*>(s)->getTag();
        if (t == T_REC) cky::recording = !cky::recording;
        if (t == T_PLAY) cky::playing = !cky::playing;
        if (t == T_STEPPER) cky::frozen = !cky::frozen;
    }

    void onBtn(CCObject* s) {
        int t = static_cast<CCNode*>(s)->getTag();
        if (t == B_SAVE) cky::save();
        if (t == B_LOAD) cky::load();
    }
};

namespace cky {
    void ckyOpenPanel(CCNode* host) {
        if (!host || host->getChildByID("cky-panel")) return;
        auto p = CkyPanel::create(); if (p) p->attachTo(host);
    }
    void ckyBuildPanel(PlayLayer* pl) { ckyOpenPanel(pl->m_uiLayer); }
}
