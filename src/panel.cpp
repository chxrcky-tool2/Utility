// panel.cpp — GDH-style menu: tab sidebar, black-outlined ON/OFF pills, +/- values, scrolling rows
#include "state.hpp"
#include <Geode/utils/web.hpp>

namespace {
    ccColor3B const kCard   = ccc3(22, 24, 40);
    ccColor3B const kSide   = ccc3(14, 15, 28);
    ccColor3B const kTab    = ccc3(30, 34, 60);
    ccColor3B const kOff    = ccc3(62, 66, 90);
    ccColor3B const kOn     = ccc3(60, 200, 120);
    ccColor3B const kAccent = ccc3(60, 130, 255);
    ccColor3B const kBtn    = ccc3(45, 52, 88);

    enum : int {
        T_REC = 1, T_PLAY, T_STEPPER, T_TPS, T_SPEED, T_AUDIO, T_FPS,
        T_NOCLIP, T_SAFE, T_NCACC, T_NCDEATH, T_HITBOX, T_HUD, T_STARTPOS, T_TELEM,
        B_SAVE = 30, B_LOAD, B_CLEAR, B_SLOT_DN, B_SLOT_UP, B_TPS_DN, B_TPS_UP, B_HIGH,
        B_SPD_DN, B_SPD_UP, B_FPS_DN, B_FPS_UP, B_HUDSZ_DN, B_HUDSZ_UP,
        B_ICONSZ_DN, B_ICONSZ_UP, B_RESETPOS, B_LICENSE, B_PACK_DN, B_PACK_UP,
        L_TPS = 70, L_SPEED, L_FPS, L_HUDSZ, L_ICONSZ, L_SLOT, L_INFO, L_INFO2, L_INFO3, L_PACK,
        S_UP = 95, S_DN = 96,
        TAB_BASE = 100
    };

    constexpr float ROW = 28.f;
    constexpr float VIEW_TOP = 90.f;
    constexpr float VIEW_BOTTOM = -112.f;
}

class CkyPanel : public CCNode {
    CCNode* m_host = nullptr;
    CCNode* m_content = nullptr;
    CCNode* m_rows = nullptr;       // everything that scrolls
    CCMenu* m_menu = nullptr;       // buttons inside m_rows
    CCMenu* m_tabMenu = nullptr;
    CCMenu* m_scrollMenu = nullptr;
    float   m_y = 0.f;
    float   m_lowest = 0.f;
    float   m_off = 0.f;
    float   m_maxOff = 0.f;
    int     m_tab = 0;
    std::vector<CCNode*> m_hidden;

public:
    static CkyPanel* create() {
        auto r = new CkyPanel();
        if (r && r->init()) { r->autorelease(); return r; }
        CC_SAFE_DELETE(r);
        return nullptr;
    }

    // hide the host's buttons so they can't be pressed through the panel
    void attachTo(CCNode* host) {
        m_host = host;
        if (auto kids = host->getChildren()) {
            for (auto c : CCArrayExt<CCNode*>(kids)) {
                if (!c->isVisible()) continue;
                if (typeinfo_cast<CCMenu*>(c) || c->getID() == "cky-icon") {
                    c->setVisible(false);
                    m_hidden.push_back(c);
                }
            }
        }
        auto win = CCDirector::get()->getWinSize();
        this->setPosition({win.width / 2.f, win.height / 2.f});
        host->addChild(this, 2000);
    }

    // black-outlined button
    CCMenuItemSpriteExtra* makeBtn(CCMenu* menu, SEL_MenuHandler cb, int tag,
                                   float x, float y, float w, float h,
                                   char const* text, ccColor3B color, float ls = 0.35f) {
        auto node = CCNode::create();
        node->setContentSize({w, h});
        node->setAnchorPoint({0.5f, 0.5f});

        auto border = CCScale9Sprite::create("square02_001.png");
        border->setContentSize({w + 4.f, h + 4.f});
        border->setColor(ccc3(0, 0, 0));
        border->setPosition({w / 2.f, h / 2.f});
        node->addChild(border, -1);

        auto bg = CCScale9Sprite::create("square02_001.png");
        bg->setContentSize({w, h});
        bg->setColor(color);
        bg->setPosition({w / 2.f, h / 2.f});
        bg->setTag(1);
        node->addChild(bg, 0);

        auto outline = CCDrawNode::create();
        CCPoint v[4] = { {0.f, 0.f}, {w, 0.f}, {w, h}, {0.f, h} };
        outline->drawPolygon(v, 4, ccc4f(0.f, 0.f, 0.f, 0.f), 1.5f, ccc4f(0.f, 0.f, 0.f, 1.f));
        node->addChild(outline, 1);

        auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
        lbl->setScale(ls);
        lbl->setPosition({w / 2.f, h / 2.f});
        lbl->setTag(2);
        node->addChild(lbl, 2);

        auto item = CCMenuItemSpriteExtra::create(node, this, cb);
        item->setTag(tag);
        item->setPosition({x, y});
        menu->addChild(item);
        return item;
    }

    static void paint(CCMenuItemSpriteExtra* it, ccColor3B c) {
        if (!it) return;
        if (auto n = it->getNormalImage()) {
            if (auto bg = typeinfo_cast<CCScale9Sprite*>(n->getChildByTag(1))) bg->setColor(c);
        }
    }

    static void retext(CCMenuItemSpriteExtra* it, std::string const& s) {
        if (!it) return;
        if (auto n = it->getNormalImage()) {
            if (auto l = typeinfo_cast<CCLabelBMFont*>(n->getChildByTag(2))) l->setString(s.c_str());
        }
    }

    CCMenuItemSpriteExtra* findBtn(int tag) {
        return m_menu ? typeinfo_cast<CCMenuItemSpriteExtra*>(m_menu->getChildByTag(tag)) : nullptr;
    }

    void setPill(int tag, bool on) {
        auto it = findBtn(tag);
        if (!it) return;
        retext(it, on ? "ON" : "OFF");
        paint(it, on ? kOn : kOff);
    }

    void setText(int tag, std::string const& s) {
        if (!m_rows) return;
        if (auto l = typeinfo_cast<CCLabelBMFont*>(m_rows->getChildByTag(tag))) {
            l->setString(s.c_str());
        }
    }

    // ---- rows ----
    void nextRow(float h = ROW) {
        m_lowest = std::min(m_lowest, m_y);
        m_y -= h;
    }

    void rowLabel(char const* text) {
        auto l = CCLabelBMFont::create(text, "bigFont.fnt");
        l->setScale(0.4f);
        l->setAnchorPoint({0.f, 0.5f});
        l->setPosition({-72.f, m_y});
        m_rows->addChild(l);
    }

    void rowPill(char const* text, int tag) {
        rowLabel(text);
        makeBtn(m_menu, menu_selector(CkyPanel::onPill), tag, 176.f, m_y, 58.f, 20.f, "OFF", kOff);
        nextRow();
    }

    void rowValue(char const* text, int dn, int up, int lbl) {
        rowLabel(text);
        makeBtn(m_menu, menu_selector(CkyPanel::onBtn), dn, 120.f, m_y, 24.f, 20.f, "-", kAccent, 0.45f);
        auto v = CCLabelBMFont::create("", "bigFont.fnt");
        v->setScale(0.4f);
        v->setPosition({152.f, m_y});
        v->setTag(lbl);
        m_rows->addChild(v);
        makeBtn(m_menu, menu_selector(CkyPanel::onBtn), up, 184.f, m_y, 24.f, 20.f, "+", kAccent, 0.45f);
        nextRow();
    }

    void rowWide(char const* text, int tag) {
        makeBtn(m_menu, menu_selector(CkyPanel::onBtn), tag, 63.f, m_y, 200.f, 22.f, text, kBtn);
        nextRow();
    }

    void rowTwo(char const* a, int ta, char const* b, int tb) {
        makeBtn(m_menu, menu_selector(CkyPanel::onBtn), ta, 0.f, m_y, 130.f, 22.f, a, kBtn);
        makeBtn(m_menu, menu_selector(CkyPanel::onBtn), tb, 140.f, m_y, 130.f, 22.f, b, kBtn);
        nextRow();
    }

    void rowInfo(int tag) {
        auto l = CCLabelBMFont::create("", "bigFont.fnt");
        l->setScale(0.35f);
        l->setAnchorPoint({0.f, 0.5f});
        l->setColor(ccc3(150, 160, 190));
        l->setPosition({-72.f, m_y});
        l->setTag(tag);
        m_rows->addChild(l);
        nextRow(22.f);
    }

    void rowText(char const* text) {
        auto l = CCLabelBMFont::create(text, "bigFont.fnt");
        l->setScale(0.3f);
        l->setAnchorPoint({0.f, 0.5f});
        l->setColor(ccc3(150, 160, 190));
        l->setPosition({-72.f, m_y});
        m_rows->addChild(l);
        nextRow(18.f);
    }

    // ---- scrolling ----
    void applyScroll() {
        m_rows->setPositionY(m_off);
        auto cull = [&](CCNode* c) {
            float y = c->getPositionY() + m_off;
            c->setVisible(y <= VIEW_TOP && y >= VIEW_BOTTOM);
        };
        if (auto kids = m_rows->getChildren()) {
            for (auto c : CCArrayExt<CCNode*>(kids)) {
                if (c != m_menu) cull(c);
            }
        }
        if (auto kids = m_menu->getChildren()) {
            for (auto c : CCArrayExt<CCNode*>(kids)) cull(c);
        }
    }

    void onScroll(CCObject* s) {
        int t = static_cast<CCNode*>(s)->getTag();
        m_off = std::clamp(m_off + (t == S_DN ? ROW : -ROW), 0.f, m_maxOff);
        applyScroll();
    }

    // ---- build ----
    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-panel");
        float W = 440.f, H = 270.f;

        // pink -> red gradient frame
        auto frame = CCLayerGradient::create(ccc4(255, 120, 190, 255), ccc4(225, 25, 50, 255));
        frame->setContentSize({W, H});
        frame->setVector({0.f, -1.f});
        frame->ignoreAnchorPointForPosition(false);
        frame->setPosition({0.f, 0.f});
        this->addChild(frame);

        auto card = CCScale9Sprite::create("square02_001.png");
        card->setContentSize({W - 12.f, H - 12.f});
        card->setColor(kCard);
        card->setOpacity(245);
        this->addChild(card, 1);

        auto side = CCScale9Sprite::create("square02_001.png");
        side->setContentSize({118.f, H - 20.f});
        side->setColor(kSide);
        side->setPosition({-149.f, 0.f});
        this->addChild(side, 2);

        auto title = CCLabelBMFont::create("ChxrCky", "bigFont.fnt");
        title->setScale(0.55f);
        title->setColor(ccc3(120, 210, 255));
        title->setPosition({-149.f, 108.f});
        this->addChild(title, 3);

        auto sub = CCLabelBMFont::create("Macro Bot", "bigFont.fnt");
        sub->setScale(0.3f);
        sub->setColor(ccc3(150, 160, 190));
        sub->setPosition({-149.f, 92.f});
        this->addChild(sub, 3);

        m_tabMenu = CCMenu::create();
        m_tabMenu->setPosition({0.f, 0.f});
        this->addChild(m_tabMenu, 4);
        static char const* names[5] = { "Bot", "Global", "Player", "Display", "Settings" };
        for (int i = 0; i < 5; ++i) {
            makeBtn(m_tabMenu, menu_selector(CkyPanel::onTab), TAB_BASE + i,
                    -149.f, 56.f - 38.f * static_cast<float>(i), 104.f, 30.f, names[i], kTab, 0.4f);
        }

        auto closeMenu = CCMenu::create();
        closeMenu->setPosition({0.f, 0.f});
        auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        closeSpr->setScale(0.55f);
        auto closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(CkyPanel::onClose));
        closeBtn->setPosition({W / 2.f - 22.f, H / 2.f - 22.f});
        closeMenu->addChild(closeBtn);
        this->addChild(closeMenu, 4);

        buildTab(0);
        return true;
    }

    void buildTab(int tab) {
        m_tab = tab;
        if (m_content) m_content->removeFromParent();
        m_content = CCNode::create();
        this->addChild(m_content, 5);

        m_rows = CCNode::create();
        m_content->addChild(m_rows, 1);
        m_menu = CCMenu::create();
        m_menu->setPosition({0.f, 0.f});
        m_rows->addChild(m_menu, 1);

        // scroll buttons (shown only when the tab has more rows than fit)
        m_scrollMenu = CCMenu::create();
        m_scrollMenu->setPosition({0.f, 0.f});
        m_content->addChild(m_scrollMenu, 2);
        makeBtn(m_scrollMenu, menu_selector(CkyPanel::onScroll), S_UP, 112.f, 113.f, 38.f, 18.f, "Up", kAccent, 0.35f);
        makeBtn(m_scrollMenu, menu_selector(CkyPanel::onScroll), S_DN, 156.f, 113.f, 38.f, 18.f, "Dn", kAccent, 0.35f);

        static char const* names[5] = { "Bot", "Global", "Player", "Display", "Settings" };
        auto head = CCLabelBMFont::create(names[tab], "bigFont.fnt");
        head->setScale(0.5f);
        head->setAnchorPoint({0.f, 0.5f});
        head->setColor(ccc3(120, 210, 255));
        head->setPosition({-72.f, 112.f});
        m_content->addChild(head);

        m_y = 80.f;
        m_lowest = 80.f;
        m_off = 0.f;
        switch (tab) {
        case 0:
            rowPill("Record", T_REC);
            rowPill("Playback", T_PLAY);
            rowPill("Frame stepper", T_STEPPER);
            rowValue("Macro slot", B_SLOT_DN, B_SLOT_UP, L_SLOT);
            rowTwo("Save macro", B_SAVE, "Load macro", B_LOAD);
            rowWide("Clear macro", B_CLEAR);
            rowInfo(L_INFO);
            rowInfo(L_INFO2);
            rowInfo(L_INFO3);
            break;
        case 1:
            rowPill("TPS bypass", T_TPS);
            rowValue("TPS", B_TPS_DN, B_TPS_UP, L_TPS);
            rowWide("High TPS", B_HIGH);
            rowPill("Speedhack", T_SPEED);
            rowValue("Speed", B_SPD_DN, B_SPD_UP, L_SPEED);
            rowPill("Audio follows speed", T_AUDIO);
            rowPill("FPS bypass", T_FPS);
            rowValue("FPS", B_FPS_DN, B_FPS_UP, L_FPS);
            break;
        case 2:
            rowPill("Noclip", T_NOCLIP);
            rowPill("Safe mode", T_SAFE);
            rowPill("Show accuracy", T_NCACC);
            rowPill("Show deaths", T_NCDEATH);
            break;
        case 3:
            rowPill("Hitboxes", T_HITBOX);
            rowPill("HUD", T_HUD);
            rowValue("HUD size", B_HUDSZ_DN, B_HUDSZ_UP, L_HUDSZ);
            rowPill("StartPos switcher", T_STARTPOS);
            break;
        default:
            rowValue("Cky icon size", B_ICONSZ_DN, B_ICONSZ_UP, L_ICONSZ);
            rowWide("Reset icon position", B_RESETPOS);
            rowPill("Checkpoint telemetry", T_TELEM);
            rowText("ChxrCky Macro Bot v1.0.2");
            rowText("(c) 2026 ChxrCky - GNU GPL v3.0");
            rowText("Free software: you may redistribute and");
            rowText("modify it under the terms of the GPL-3.0.");
            rowText("NO WARRANTY. Full license text:");
            rowWide("Open GPL-3.0 license", B_LICENSE);
            break;
        }

        float needed = VIEW_BOTTOM - m_lowest;
        m_maxOff = needed > 0.f ? std::ceil(needed / ROW) * ROW : 0.f;
        m_scrollMenu->setVisible(m_maxOff > 0.f);
        applyScroll();
        refresh();
        refreshTabs();
    }

    void refreshTabs() {
        for (int i = 0; i < 5; ++i) {
            auto it = typeinfo_cast<CCMenuItemSpriteExtra*>(m_tabMenu->getChildByTag(TAB_BASE + i));
            paint(it, i == m_tab ? kAccent : kTab);
        }
    }

    void refresh() {
        setPill(T_REC, cky::recording);
        setPill(T_PLAY, cky::playing);
        setPill(T_STEPPER, cky::frozen);
        setPill(T_TPS, cky::tpsOn);
        setPill(T_SPEED, cky::speedOn);
        setPill(T_AUDIO, cky::audioSpeed);
        setPill(T_FPS, cky::fpsOn);
        setPill(T_NOCLIP, cky::noclip);
        setPill(T_SAFE, cky::safeMode);
        setPill(T_NCACC, cky::ncShowAcc);
        setPill(T_NCDEATH, cky::ncShowDeaths);
        setPill(T_HITBOX, cky::hitboxes);
        setPill(T_HUD, cky::hudOn);
        setPill(T_STARTPOS, cky::startPosOn);
        setPill(T_TELEM, cky::ckptTelemetry);

        setText(L_TPS, fmt::format("{}", static_cast<int>(cky::tps)));
        setText(L_SPEED, fmt::format("{:.2f}x", cky::speed));
        setText(L_FPS, fmt::format("{}", static_cast<int>(cky::fps)));
        setText(L_HUDSZ, fmt::format("{:.1f}x", cky::hudScale));
        setText(L_ICONSZ, fmt::format("{:.1f}x", cky::iconScale));
        setText(L_SLOT, fmt::format("{}", cky::slot));
        setText(L_INFO, fmt::format("Inputs: {}", cky::macro.size()));
        setText(L_INFO2, fmt::format("Macro TPS: {}", static_cast<int>(cky::macroTps)));
        setText(L_INFO3, fmt::format("Slot {}: {}", cky::slot,
            cky::slotExists() ? "saved" : "empty"));

        retext(findBtn(B_HIGH), (cky::tpsOn && cky::tps >= 480.f)
            ? fmt::format("High TPS: {} (tap to cycle)", static_cast<int>(cky::tps))
            : std::string("High TPS: OFF (tap to cycle)"));
    }

    // ---- handlers ----
    void onTab(CCObject* s) {
        buildTab(static_cast<CCNode*>(s)->getTag() - TAB_BASE);
    }

    void onClose(CCObject*) {
        for (auto n : m_hidden) n->setVisible(true);
        m_hidden.clear();
        this->removeFromParent();
    }

    void onPill(CCObject* s) {
        switch (static_cast<CCNode*>(s)->getTag()) {
        case T_REC:
            if (cky::recording) {
                cky::recording = false;
                cky::recArmed = false;
            } else {
                cky::recording = true;
                cky::playing = false;
                cky::playArmed = false;
                cky::macroTps = cky::effTps();
                cky::recArmed = (cky::step == 0);
                if (!cky::recArmed) ckyNotify("Recording starts when you restart the level");
            }
            break;
        case T_PLAY:
            if (cky::playing) {
                cky::playing = false;
                cky::playArmed = false;
            } else if (cky::macro.empty()) {
                ckyNotify("No macro loaded", false);
            } else {
                cky::playing = true;
                cky::recording = false;
                cky::recArmed = false;
                cky::cursor = 0;
                cky::playArmed = (cky::step == 0);
                if (!cky::playArmed) ckyNotify("Playback ON: restart the level to start it");
            }
            break;
        case T_STEPPER:
            cky::frozen = !cky::frozen;
            cky::pending = 0;
            ckyApplyAudio();
            break;
        case T_TPS:
            if (cky::recording) { ckyNotify("Stop recording first", false); break; }
            cky::tpsOn = !cky::tpsOn;
            cky::resetMeasure();
            break;
        case T_SPEED:
            cky::speedOn = !cky::speedOn;
            ckyApplyAudio();
            break;
        case T_AUDIO:
            cky::audioSpeed = !cky::audioSpeed;
            ckyApplyAudio();
            break;
        case T_FPS:
            cky::fpsOn = !cky::fpsOn;
            ckyApplyFps();
            break;
        case T_NOCLIP:
            cky::noclip = !cky::noclip;
            cky::ncReset();
            break;
        case T_SAFE:
            cky::safeMode = !cky::safeMode;
            break;
        case T_NCACC:
            cky::ncShowAcc = !cky::ncShowAcc;
            break;
        case T_NCDEATH:
            cky::ncShowDeaths = !cky::ncShowDeaths;
            break;
        case T_HITBOX:
            cky::hitboxes = !cky::hitboxes;
            ckyApplyHitboxes();
            break;
        case T_HUD:
            cky::hudOn = !cky::hudOn;
            if (auto h = ckyHudNode()) h->setVisible(cky::hudOn);
            break;
        case T_STARTPOS:
            cky::startPosOn = !cky::startPosOn;
            ckySetStartPosVisible(cky::startPosOn);
            break;
        case T_TELEM:
            cky::ckptTelemetry = !cky::ckptTelemetry;
            break;
        default: break;
        }
        refresh();
        ckyHudRefresh();
    }

    void onBtn(CCObject* s) {
        int t = static_cast<CCNode*>(s)->getTag();
        switch (t) {
        case B_SAVE: {
            bool ok = cky::save();
            ckyNotify(ok ? fmt::format("Saved to slot {}", cky::slot).c_str() : "Save failed", ok);
            break;
        }
        case B_LOAD: {
            bool ok = cky::load();
            ckyNotify(ok ? fmt::format("Loaded slot {}", cky::slot).c_str() : "Load failed", ok);
            break;
        }
        case B_CLEAR:
            cky::clearMacro();
            ckyNotify("Macro buffer cleared");
            break;
        case B_SLOT_DN:
        case B_SLOT_UP:
            cky::slot = std::clamp(cky::slot + (t == B_SLOT_UP ? 1 : -1), 1, cky::MAX_SLOTS);
            break;
        case B_TPS_DN:
        case B_TPS_UP:
            if (cky::recording) { ckyNotify("Stop recording first", false); break; }
            cky::tps = cky::nextVal(cky::tpsList, cky::tps, t == B_TPS_UP ? 1 : -1);
            cky::tpsOn = true;
            cky::resetMeasure();
            break;
        case B_HIGH:
            if (cky::recording) { ckyNotify("Stop recording first", false); break; }
            cky::tps = cky::nextHighTps(cky::tpsOn ? cky::tps : 240.f);
            cky::tpsOn = true;
            cky::resetMeasure();
            break;
        case B_SPD_DN:
        case B_SPD_UP:
            cky::speed = cky::nextVal(cky::speedList, cky::speed, t == B_SPD_UP ? 1 : -1);
            cky::speedOn = true;
            ckyApplyAudio();
            break;
        case B_FPS_DN:
        case B_FPS_UP:
            cky::fps = cky::nextVal(cky::fpsList, cky::fps, t == B_FPS_UP ? 1 : -1);
            cky::fpsOn = true;
            ckyApplyFps();
            break;
        case B_HUDSZ_DN:
        case B_HUDSZ_UP:
            cky::hudScale = cky::nextVal(cky::sizeList, cky::hudScale, t == B_HUDSZ_UP ? 1 : -1);
            ckyApplyHudScale();
            break;
        case B_ICONSZ_DN:
        case B_ICONSZ_UP:
            cky::iconScale = cky::nextVal(cky::sizeList, cky::iconScale, t == B_ICONSZ_UP ? 1 : -1);
            if (m_host) {
                if (auto ic = m_host->getChildByID("cky-icon")) ic->setScale(cky::iconScale);
            }
            break;
        case B_RESETPOS:
            cky::savePos("cky-icon", CCPoint(-99999.f, -99999.f));
            if (m_host) {
                if (auto ic = m_host->getChildByID("cky-icon")) ic->setPosition(ckyDefaultIconPos());
            }
            break;
        case B_PACK_DN:
        case B_PACK_UP:
            ckyClickPackStep(t == B_PACK_UP ? 1 : -1);
            break;
        case B_LICENSE:
            web::openLinkInBrowser("https://www.gnu.org/licenses/gpl-3.0.html");
            break;
        default: break;
        }
        refresh();
        ckyHudRefresh();
    }
};

void ckyOpenPanel(CCNode* host) {
    if (!host || host->getChildByID("cky-panel")) return;
    auto p = CkyPanel::create();
    if (!p) return;
    p->attachTo(host);
}

// END OF FILE (panel.cpp)
