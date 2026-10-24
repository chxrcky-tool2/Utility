#include "state.hpp"
#include <Geode/utils/web.hpp>
#include <Amber.hpp>

using namespace amber;

class CkyPanel : public Popup<> {
    int m_currentTab = 0;
    CCNode* m_content = nullptr;

public:
    static CkyPanel* create() {
        auto ret = new CkyPanel();
        if (ret && ret->init(440.f, 270.f)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool setup() override {
        this->setID("cky-panel");
        this->setTitle("CHXRCKY");

        auto sideMenu = CCMenu::create();
        sideMenu->setPosition({-150.f, 0.f});
        m_mainLayer->addChild(sideMenu);

        auto tabs = {"Bot", "Player", "Visual", "Settings"};
        float y = 80.f;
        for (int i = 0; i < 4; ++i) {
            auto label = CCLabelBMFont::create(tabs[i], "bigFont.fnt");
            label->setScale(0.5f);
            auto btn = CCMenuItemSpriteExtra::create(label, this, menu_selector(CkyPanel::onTab));
            btn->setTag(i);
            btn->setPosition({0.f, y});
            sideMenu->addChild(btn);
            y -= 40.f;
        }

        buildTab(0);
        return true;
    }

    void buildTab(int tab) {
        m_currentTab = tab;
        if (m_content) m_content->removeFromParent();
        m_content = CCNode::create();
        m_content->setPosition({20.f, 0.f});
        m_mainLayer->addChild(m_content);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        m_content->addChild(menu);

        auto head = CCLabelBMFont::create(tab == 0 ? "Bot" : tab == 1 ? "Player" : tab == 2 ? "Visual" : "Settings", "bigFont.fnt");
        head->setScale(0.6f);
        head->setPosition({80.f, 110.f});
        m_content->addChild(head);

        float y = 70.f;
        if (tab == 0) {
            makeBtn(menu, menu_selector(CkyPanel::onToggle), 1, y, "RECORD"); y -= 35.f;
            makeBtn(menu, menu_selector(CkyPanel::onToggle), 2, y, "PLAYBACK"); y -= 35.f;
            makeBtn(menu, menu_selector(CkyPanel::onToggle), 3, y, "FRAME STEPPER"); y -= 35.f;
        } else if (tab == 1) {
            makeBtn(menu, menu_selector(CkyPanel::onToggle), 10, y, "NOCLIP"); y -= 35.f;
        } else if (tab == 2) {
            makeBtn(menu, menu_selector(CkyPanel::onToggle), 11, y, "SHOW HITBOXES"); y -= 35.f;
        } else if (tab == 3) {
            auto label = CCLabelBMFont::create("GPL-3.0 License", "bigFont.fnt");
            label->setScale(0.4f);
            auto btn = CCMenuItemSpriteExtra::create(label, this, menu_selector(CkyPanel::onBtn));
            btn->setTag(12);
            btn->setPosition({80.f, y});
            menu->addChild(btn);
        }
    }

    void makeBtn(CCMenu* menu, SEL_MenuHandler cb, int tag, float y, const char* text) {
        auto bg = CCScale9Sprite::create("square02_001.png");
        bg->setContentSize({200.f, 25.f});
        bg->setColor({90, 50, 25});
        
        auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
        lbl->setScale(0.4f);
        lbl->setPosition({100.f, 12.5f});
        bg->addChild(lbl);
        
        auto btn = CCMenuItemSpriteExtra::create(bg, this, cb);
        btn->setTag(tag);
        btn->setPosition({80.f, y});
        menu->addChild(btn);
    }

    void onTab(CCObject* s) {
        buildTab(static_cast<CCNode*>(s)->getTag());
    }

    void onToggle(CCObject* s) {
        int t = static_cast<CCNode*>(s)->getTag();
        if (t == 1) cky::recording = !cky::recording;
        if (t == 2) cky::playing = !cky::playing;
        if (t == 3) cky::frozen = !cky::frozen;
        if (t == 10) cky::noclip = !cky::noclip;
        if (t == 11) cky::hitboxes = !cky::hitboxes;
    }

    void onBtn(CCObject*) {
        web::openLinkInBrowser("https://www.gnu.org/licenses/gpl-3.0.html");
    }
};

namespace cky {
    void ckyOpenPanel(CCNode* host) {
        if (!host) return;
        if (host->getChildByID("cky-panel")) return;
        auto panel = CkyPanel::create();
        if (panel) panel->show();
    }
    void ckyBuildPanel(PlayLayer* pl) { ckyOpenPanel(pl->m_uiLayer); }
}
