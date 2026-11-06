// panel.cpp — menu: themes, 6 tabs, black-outlined pills, +/- values, scrolling rows
#include "state.hpp"
#include <Geode/utils/web.hpp>

namespace {
    struct Theme {
        ccColor3B card, side, tab, off, on, accent, btn, title;
        ccColor4B top, bottom;
        char const* name;
    };

    Theme const kThemes[] = {
        { ccc3(22,24,40), ccc3(14,15,28), ccc3(30,34,60), ccc3(62,66,90), ccc3(60,200,120),
          ccc3(60,130,255), ccc3(45,52,88), ccc3(120,210,255),
          ccc4(255,120,190,255), ccc4(225,25,50,255), "Neon" },
        { ccc3(9,34,50), ccc3(5,24,38), ccc3(15,58,80), ccc3(46,76,94), ccc3(64,226,190),
          ccc3(0,170,220), ccc3(18,82,108), ccc3(150,244,242),
          ccc4(72,226,210,255), ccc4(6,66,130,255), "Ocean" },
        { ccc3(18,32,24), ccc3(12,24,18), ccc3(28,50,36), ccc3(58,76,64), ccc3(110,210,90),
          ccc3(60,170,110), ccc3(34,62,44), ccc3(170,240,150),
          ccc4(120,220,120,255), ccc4(20,110,70,255), "Forest" },
        { ccc3(38,22,30), ccc3(28,14,22), ccc3(60,34,44), ccc3(88,62,70), ccc3(255,170,60),
          ccc3(255,100,90), ccc3(74,40,52), ccc3(255,200,140),
          ccc4(255,190,70,255), ccc4(220,50,90,255), "Sunset" },
        { ccc3(24,24,26), ccc3(16,16,18), ccc3(40,40,44), ccc3(70,70,76), ccc3(200,200,206),
          ccc3(130,130,140), ccc3(52,52,58), ccc3(230,230,235),
          ccc4(200,200,206,255), ccc4(70,70,76,255), "Mono" },
        // Cat: black cat, amber eyes, pink nose
        { ccc3(28,26,32), ccc3(18,17,22), ccc3(48,44,56), ccc3(84,78,92), ccc3(255,196,50),
          ccc3(255,140,170), ccc3(58,52,66), ccc3(255,214,120),
          ccc4(255,196,70,255), ccc4(70,56,84,255), "Cat" },
        // Vintage: sepia / old paper
        { ccc3(46,38,30), ccc3(34,28,22), ccc3(70,58,44), ccc3(104,92,76), ccc3(154,184,104),
          ccc3(200,154,84), ccc3(84,68,48), ccc3(236,218,174),
          ccc4(218,188,134,255), ccc4(122,80,50,255), "Vintage" },
        // Sky
        { ccc3(24,56,94), ccc3(16,42,74), ccc3(36,82,128), ccc3(84,114,146), ccc3(120,226,255),
          ccc3(96,184,255), ccc3(42,94,148), ccc3(206,240,255),
          ccc4(156,218,255,255), ccc4(72,152,232,255), "Sky" },
        // Lavender purple
        { ccc3(46,36,78), ccc3(34,26,62), ccc3(68,54,108), ccc3(104,94,134), ccc3(204,174,255),
          ccc3(174,134,255), ccc3(80,64,124), ccc3(228,210,255),
          ccc4(218,194,255,255), ccc4(132,92,222,255), "Lavender" },
        // Rose (light pink)
        { ccc3(78,42,60), ccc3(62,32,48), ccc3(108,60,84), ccc3(140,104,120), ccc3(255,176,206),
          ccc3(255,134,174), ccc3(124,64,94), ccc3(255,228,238),
          ccc4(255,210,228,255), ccc4(240,142,182,255), "Rose" },
    };

    static_assert(sizeof(kThemes) / sizeof(kThemes[0]) == cky::ThemeCount, "theme tables out of sync");

    Theme const& thm() {
        int n = static_cast<int>(sizeof(kThemes) / sizeof(kThemes[0]));
        return kThemes[std::clamp(cky::theme, 0, n - 1)];
    }

    int g_startTab = 0;

#define kCard   (thm().card)
#define kSide   (thm().side)
#define kTab    (thm().tab)
#define kOff    (thm().off)
#define kOn     (thm().on)
#define kAccent (thm().accent)
#define kBtn    (thm().btn)

    enum : int {
        T_REC = 1, T_PLAY, T_STEPPER, T_TPS, T_SPEED, T_AUDIO,
        T_NOCLIP, T_SAFE, T_NCACC, T_NCDEATH, T_HITBOX, T_HUD, T_STARTPOS, T_TELEM,
        T_OVERLAY, T_CLICKHUD, T_CLICKBOT, T_TRAJ, T_FFIX,
        T_HUDSTEP, T_HUDSPEED, T_BACKSTEP, T_BACKALWAYS, T_STEPBTNS, T_SPEEDBTNS,
        B_SAVE = 30, B_LOAD, B_CLEAR, B_SLOT_DN, B_SLOT_UP, B_TPS_DN, B_TPS_UP, B_HIGH,
        B_TSTEP_DN, B_TSTEP_UP, B_SPD_DN, B_SPD_UP, B_HUDSZ_DN, B_HUDSZ_UP,
        B_ICONSZ_DN, B_ICONSZ_UP, B_RESETPOS, B_LICENSE, B_PACK_DN, B_PACK_UP, B_IMPORT,
        B_THEME_DN, B_THEME_UP, B_OFF_DN, B_OFF_UP, B_VOL_DN, B_VOL_UP, B_TLEN_DN, B_TLEN_UP,
        B_HUDGEAR, B_HUDBACK, B_FI_DN, B_FI_UP, B_FVEL,
        L_TPS = 70, L_TSTEP, L_SPEED, L_HUDSZ, L_ICONSZ, L_SLOT, L_INFO, L_INFO2, L_INFO3,
        L_PACK, L_THEME, L_OFF, L_VOL, L_TLEN, L_FI,
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

    // small bordered button with a drawn gear on it
    CCMenuItemSpriteExtra* makeGear(CCMenu* menu, int tag, float x, float y) {
        auto it = makeBtn(menu, menu_selector(CkyPanel::onBtn), tag, x, y, 24.f, 20.f, "", kBtn);
        if (auto n = it->getNormalImage()) {
            auto g = CCDrawNode::create();
            float cx = 12.f, cy = 10.f;
            ccColor4F glyph = ccc4f(0.88f, 1.f, 1.f, 1.f);
            ccColor3B b = kBtn;
            ccColor4F hole = ccc4f(b.r / 255.f, b.g / 255.f, b.b / 255.f, 1.f);
            for (int i = 0; i < 8; ++i) {
                float a = static_cast<float>(i) * 3.14159265f / 4.f;
                float dx = std::cos(a), dy = std::sin(a);
                float px = -dy, py = dx; // perpendicular
                float r0 = 4.f, r1 = 8.2f, w = 1.5f;
                CCPoint q[4] = {
                    { cx + dx * r0 + px * w, cy + dy * r0 + py * w },
                    { cx + dx * r1 + px * w, cy + dy * r1 + py * w },
                    { cx + dx * r1 - px * w, cy + dy * r1 - py * w },
                    { cx + dx * r0 - px * w, cy + dy * r0 - py * w }
                };
                g->drawPolygon(q, 4, glyph, 0.f, glyph);
            }
            g->drawDot({ cx, cy }, 5.6f, glyph);
            g->drawDot({ cx, cy }, 2.3f, hole);
            n->addChild(g, 3);
        }
        return it;
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
        cky::theme = Mod::get()->getSavedValue<int>("theme", 0);
        float W = 440.f, H = 270.f;

        auto frame = CCLayerGradient::create(thm().top, thm().bottom);
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
        title->setColor(thm().title);
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
        static char const* names[6] = { "Bot", "Global", "Player", "Display", "Clicks", "Settings" };
        for (int i = 0; i < 6; ++i) {
            makeBtn(m_tabMenu, menu_selector(CkyPanel::onTab), TAB_BASE + i,
                    -149.f, 60.f - 32.f * static_cast<float>(i), 104.f, 28.f, names[i], kTab, 0.4f);
        }

        auto closeMenu = CCMenu::create();
        closeMenu->setPosition({0.f, 0.f});
        auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        closeSpr->setScale(0.55f);
        auto closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(CkyPanel::onClose));
        closeBtn->setPosition({W / 2.f - 22.f, H / 2.f - 22.f});
        closeMenu->addChild(closeBtn);
        this->addChild(closeMenu, 4);

        buildTab(g_startTab);
        g_startTab = 0;
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

        static char const* names[7] = { "Bot", "Global", "Player", "Display", "Clicks", "Settings",
                                        "HUD settings" };
        auto head = CCLabelBMFont::create(names[tab], "bigFont.fnt");
        head->setScale(0.5f);
        head->setAnchorPoint({0.f, 0.5f});
        head->setColor(thm().title);
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
            rowPill("Frame fixes", T_FFIX);
            rowValue("Fix interval", B_FI_DN, B_FI_UP, L_FI);
            rowWide("Toggle fix velocity", B_FVEL);
            rowValue("Macro slot", B_SLOT_DN, B_SLOT_UP, L_SLOT);
            rowTwo("Save macro", B_SAVE, "Load macro", B_LOAD);
            rowWide("Clear macro", B_CLEAR);
            rowWide("Import GDR file", B_IMPORT);
            rowValue("Input offset", B_OFF_DN, B_OFF_UP, L_OFF);
            rowInfo(L_INFO);
            rowInfo(L_INFO2);
            rowInfo(L_INFO3);
            break;
        case 1:
            rowPill("TPS bypass", T_TPS);
            rowValue("TPS (1-6000)", B_TPS_DN, B_TPS_UP, L_TPS);
            rowValue("TPS step", B_TSTEP_DN, B_TSTEP_UP, L_TSTEP);
            rowWide("High TPS", B_HIGH);
            rowPill("Speedhack", T_SPEED);
            rowValue("Speed", B_SPD_DN, B_SPD_UP, L_SPEED);
            rowPill("Audio follows speed", T_AUDIO);
            break;
        case 2:
            rowPill("Noclip", T_NOCLIP);
            rowPill("Safe mode", T_SAFE);
            rowPill("Show accuracy", T_NCACC);
            rowPill("Show deaths", T_NCDEATH);
            break;
        case 3:
            rowPill("Hitboxes", T_HITBOX);
            {
                float hy = m_y;
                rowPill("HUD", T_HUD);
                makeGear(m_menu, B_HUDGEAR, 122.f, hy);
            }
            rowValue("HUD size", B_HUDSZ_DN, B_HUDSZ_UP, L_HUDSZ);
            rowPill("StartPos switcher", T_STARTPOS);
            rowPill("Input overlay", T_OVERLAY);
            rowPill("Click telemetry", T_CLICKHUD);
            rowPill("Show trajectory", T_TRAJ);
            rowValue("Trajectory length", B_TLEN_DN, B_TLEN_UP, L_TLEN);
            break;
        case 4:
            rowPill("Clickbot sounds", T_CLICKBOT);
            rowValue("Clickpack", B_PACK_DN, B_PACK_UP, L_PACK);
            rowValue("Volume", B_VOL_DN, B_VOL_UP, L_VOL);
            rowText("Packs go in game/clicks/<PackName>/");
            rowText("with player1/clicks, releases, softclicks...");
            break;
        case 6:
            rowPill("Show frame stepper", T_HUDSTEP);
            rowPill("Show speedhack", T_HUDSPEED);
            rowPill("Frame stepper buttons", T_STEPBTNS);
            rowPill("Speedhack buttons", T_SPEEDBTNS);
            rowPill("Back step", T_BACKSTEP);
            rowPill("Buffer while playing", T_BACKALWAYS);
            rowWide("Back to Display", B_HUDBACK);
            break;
        default:
            rowValue("Cky icon size", B_ICONSZ_DN, B_ICONSZ_UP, L_ICONSZ);
            rowWide("Reset icon position", B_RESETPOS);
            rowPill("Checkpoint telemetry", T_TELEM);
            rowValue("Theme", B_THEME_DN, B_THEME_UP, L_THEME);
            rowText("ChxrCky Macro Bot v1.0.5");
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
        for (int i = 0; i < 6; ++i) {
            auto it = typeinfo_cast<CCMenuItemSpriteExtra*>(m_tabMenu->getChildByTag(TAB_BASE + i));
            paint(it, i == (m_tab == 6 ? 3 : m_tab) ? kAccent : kTab);
        }
    }

    void refresh() {
        setPill(T_REC, cky::recording);
        setPill(T_PLAY, cky::playing);
        setPill(T_STEPPER, cky::frozen);
        setPill(T_TPS, cky::tpsOn);
        setPill(T_SPEED, cky::speedOn);
        setPill(T_AUDIO, cky::audioSpeed);
        setPill(T_NOCLIP, cky::noclip);
        setPill(T_SAFE, cky::safeMode);
        setPill(T_NCACC, cky::ncShowAcc);
        setPill(T_NCDEATH, cky::ncShowDeaths);
        setPill(T_HITBOX, cky::hitboxes);
        setPill(T_HUD, cky::hudOn);
        setPill(T_STARTPOS, cky::startPosOn);
        setPill(T_TELEM, cky::ckptTelemetry);
        setPill(T_OVERLAY, cky::overlayOn);
        setPill(T_CLICKHUD, cky::clickHudOn);
        setPill(T_CLICKBOT, cky::clickbotOn);
        setPill(T_TRAJ, cky::trajectory);
        setPill(T_FFIX, cky::frameFixes);
        setPill(T_HUDSTEP, cky::hudStepShow);
        setPill(T_HUDSPEED, cky::hudSpeedShow);
        setPill(T_BACKSTEP, cky::backStepOn);
        setPill(T_STEPBTNS, cky::stepperBtns);
        setPill(T_SPEEDBTNS, cky::speedBtns);
        setPill(T_BACKALWAYS, cky::backAlways);
        setText(L_TLEN, fmt::format("{}", cky::trajLen));
        {
            std::string n = ckyClickPackName();
            if (n.size() > 9) n = n.substr(0, 8) + "~";
            setText(L_PACK, n);
        }

        setText(L_TPS, fmt::format("{}", static_cast<int>(cky::tps)));
        setText(L_TSTEP, fmt::format("{}", static_cast<int>(cky::tpsStep)));
        setText(L_SPEED, fmt::format("{:.2f}x", cky::speed));
        setText(L_HUDSZ, fmt::format("{:.1f}x", cky::hudScale));
        setText(L_ICONSZ, fmt::format("{:.1f}x", cky::iconScale));
        setText(L_SLOT, fmt::format("{}", cky::slot));
        setText(L_VOL, fmt::format("{}%", cky::clickVol));
        setText(L_INFO, fmt::format("Inputs: {}", cky::macro.size()));
        setText(L_INFO2, fmt::format("Macro TPS: {}", static_cast<int>(cky::macroTps)));
        setText(L_INFO3, fmt::format("Slot {}: {}", cky::slot,
            cky::slotExists() ? "saved" : "empty"));
        setText(L_OFF, fmt::format("{:+d}", cky::frameOffset));
        setText(L_FI, fmt::format("{}", cky::fixInterval));
        setText(L_THEME, thm().name);

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
        case T_OVERLAY:
            cky::overlayOn = !cky::overlayOn;
            break;
        case T_CLICKHUD:
            cky::clickHudOn = !cky::clickHudOn;
            break;
        case T_CLICKBOT:
            cky::clickbotOn = !cky::clickbotOn;
            ckyClickRescan();
            break;
        case T_TRAJ:
            cky::trajectory = !cky::trajectory;
            if (!cky::trajectory) ckyTrajClear();
            else cky::dirty = true; // trajectory counts as a cheat: completion is blocked in safe mode
            break;
        case T_FFIX:
            if (cky::recording) { ckyNotify("Stop recording first", false); break; }
            cky::frameFixes = !cky::frameFixes;
            break;
        case T_HUDSTEP:
            cky::hudStepShow = !cky::hudStepShow;
            break;
        case T_HUDSPEED:
            cky::hudSpeedShow = !cky::hudSpeedShow;
            break;
        case T_STEPBTNS:
            cky::stepperBtns = !cky::stepperBtns;
            if (!cky::stepperBtns) {      // no buttons left to unpause with: resume
                cky::frozen = false;
                cky::pending = 0;
                ckyApplyAudio();
            }
            break;
        case T_SPEEDBTNS:
            cky::speedBtns = !cky::speedBtns;
            ckyApplyAudio();
            break;
        case T_BACKSTEP:
            cky::backStepOn = !cky::backStepOn;
            if (!cky::backStepOn) ckyBackStepClear();
            break;
        case T_BACKALWAYS:
            cky::backAlways = !cky::backAlways;
            if (!cky::backAlways && !cky::frozen) ckyBackStepClear();
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
            cky::tps = std::clamp(cky::tps + (t == B_TPS_UP ? cky::tpsStep : -cky::tpsStep),
                cky::TPS_MIN, cky::TPS_MAX);
            cky::tpsOn = true;
            cky::resetMeasure();
            break;
        case B_TSTEP_DN:
        case B_TSTEP_UP:
            cky::tpsStep = cky::nextVal(cky::tpsStepList, cky::tpsStep, t == B_TSTEP_UP ? 1 : -1);
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
        case B_VOL_DN:
        case B_VOL_UP:
            cky::clickVol = std::clamp(cky::clickVol + (t == B_VOL_UP ? 10 : -10), 0, 200);
            Mod::get()->setSavedValue<int>("click-vol", cky::clickVol);
            break;
        case B_TLEN_DN:
        case B_TLEN_UP:
            cky::trajLen = std::clamp(cky::trajLen + (t == B_TLEN_UP ? 20 : -20), 60, 480);
            break;
        case B_FI_DN:
        case B_FI_UP:
            if (cky::recording) { ckyNotify("Stop recording first", false); break; }
            cky::fixInterval = static_cast<int>(cky::nextVal(
                cky::fixIntervalList, static_cast<float>(cky::fixInterval), t == B_FI_UP ? 1 : -1));
            break;
        case B_FVEL:
            cky::fixVelocity = !cky::fixVelocity;
            ckyNotify(cky::fixVelocity ? "Fix velocity: on" : "Fix velocity: off");
            break;
        case B_OFF_DN:
        case B_OFF_UP:
            cky::frameOffset = std::clamp(cky::frameOffset + (t == B_OFF_UP ? 1 : -1), -10, 10);
            break;
        case B_IMPORT:
            ckyImportGdr();
            break;
        case B_THEME_DN:
        case B_THEME_UP: {
            int n = static_cast<int>(sizeof(kThemes) / sizeof(kThemes[0]));
            cky::theme = ((cky::theme + (t == B_THEME_UP ? 1 : -1)) % n + n) % n;
            Mod::get()->setSavedValue<int>("theme", cky::theme);
            g_startTab = m_tab;
            auto host = m_host;
            this->onClose(nullptr);
            ckyOpenPanel(host);
            return;
        }
        case B_HUDGEAR:
            buildTab(6);
            return;
        case B_HUDBACK:
            buildTab(3);
            return;
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
    if (ckyUiIsImGui()) { ckyImGuiToggle(); return; }
    if (!host || host->getChildByID("cky-panel")) return;
    auto p = CkyPanel::create();
    if (!p) return;
    p->attachTo(host);
}

// END OF FILE (panel.cpp)
