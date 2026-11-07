// panel.cpp — menu: themes (see themes.hpp), 6 tabs, black-outlined pills, +/- values, scrolling rows
#include "state.hpp"
#include "fx.hpp"
#include <Geode/utils/web.hpp>

namespace {
    // Cocos-side view of a theme (built once from cky::kThemeDefs in themes.hpp)
    struct Theme {
        ccColor3B card, side, tab, off, on, accent, btn, title;
        ccColor4B top, bottom;
        char const* name;
    };

    ccColor3B c3(unsigned char const (&v)[3]) { return ccc3(v[0], v[1], v[2]); }
    ccColor4B c4(unsigned char const (&v)[3]) { return ccc4(v[0], v[1], v[2], 255); }

    Theme const& thm() {
        static std::vector<Theme> const themes = [] {
            std::vector<Theme> out;
            out.reserve(cky::ThemeCount);
            for (auto const& d : cky::kThemeDefs) {
                out.push_back({ c3(d.card), c3(d.side), c3(d.tab), c3(d.off), c3(d.on),
                                c3(d.accent), c3(d.btn), c3(d.title),
                                c4(d.top), c4(d.bottom), d.name });
            }
            return out;
        }();
        return themes[std::clamp(cky::theme, 0, cky::ThemeCount - 1)];
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
        T_HUDSTEP, T_HUDSPEED, T_BACKSTEP, T_BACKALWAYS, T_STEPBTNS, T_SPEEDBTNS, T_TRAIL, T_PFIX,
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

    // hitbox trail page (kept above the other tag ranges)
    enum : int {
        B_TR_GEAR = 200, B_TR_BACK, B_TRL_DN, B_TRL_UP,
        B_TRC_BASE = 210,   // 210..225: [which*8 + channel*2 + (up ? 1 : 0)], which 0 = outer, 1 = inner
        L_TRL = 230,
        L_TRC_BASE = 231,   // 231..238: [which*4 + channel]
        L_SW_OUT = 240, L_SW_IN
    };

    // Labels tab (kept above the other tag ranges). The tab itself is page 8; its button sits
    // between Display and Clicks, so page numbers 6 / 7 (HUD settings / Hitbox trail) stay as they are.
    enum : int {
        T_LBL_FPS = 250, T_LBL_RUN, T_LBL_TIME, T_LBL_BEST,
        PAGE_LABELS = 8
    };

    constexpr float ROW = 28.f;
    constexpr float VIEW_TOP = 90.f;
    constexpr float VIEW_BOTTOM = -112.f;

    // ================= theme effects (Cat / Molten rocks) — maths lives in fx.hpp =================
    using cky::fx::Kind;
    Kind fxKind() { return cky::fx::kindOf(cky::theme); }

    // CCDrawNode blends premultiplied, so every translucent colour goes through these
    ccColor4F pm(float r, float g, float b, float a = 1.f) { return ccc4f(r * a, g * a, b * a, a); }
    ccColor4F pm3(ccColor3B c, float a = 1.f) { return pm(c.r / 255.f, c.g / 255.f, c.b / 255.f, a); }
    ccColor4F pmc(cky::fx::Col c, float a = 1.f) { return pm(c.r, c.g, c.b, a); }

    constexpr float PI_F = 3.14159265f;

    // rounded rectangle, 4 segments per corner -> 20 points
    int roundRect(CCPoint* out, float x0, float y0, float x1, float y1, float r) {
        int n = 0;
        float cx[4] = { x1 - r, x0 + r, x0 + r, x1 - r };
        float cy[4] = { y1 - r, y1 - r, y0 + r, y0 + r };
        for (int c = 0; c < 4; ++c) {
            for (int i = 0; i <= 4; ++i) {
                float a = (static_cast<float>(c) * 90.f + static_cast<float>(i) * 22.5f) * PI_F / 180.f;
                out[n++] = { cx[c] + std::cos(a) * r, cy[c] + std::sin(a) * r };
            }
        }
        return n;
    }

    // chamfered ("hexagonal") corners: 8 points
    void chamferPts(CCPoint* o, float x0, float y0, float x1, float y1, float c) {
        float f[8][2];
        cky::fx::chamfer(x0, y0, x1, y1, c, f);
        for (int i = 0; i < 8; ++i) o[i] = { f[i][0], f[i][1] };
    }

    void drawPawCC(CCDrawNode* d, float x, float y, float s, float rot, ccColor4F col) {
        cky::fx::pawDots(x, y, s, rot, [&](float px, float py, float r) { d->drawDot({ px, py }, r, col); });
    }

    // Molten rocks button: sharp chamfered corners, hot orange rim, lit top edge
    void drawLavaBtn(CCDrawNode* d, float w, float h, ccColor3B c) {
        d->clear();
        float k = std::min(h * 0.3f, 6.f);
        CCPoint v[8];
        chamferPts(v, 0.f, 0.f, w, h, k);
        d->drawPolygon(v, 8, pm3(c), 1.5f, pm(1.f, 0.55f, 0.16f, 0.95f));
        d->drawSegment({ k + 1.f, h - 1.f }, { w - k - 1.f, h - 1.f }, 0.4f, pm(1.f, 0.78f, 0.4f, 0.35f));
    }

    // ---- toggle boxes (26x22). Cat = cat-face box with ears + paw check. Lava = magma box + orange check ----
    constexpr float TOG_W = 26.f, TOG_H = 22.f;

    void drawToggleBox(CCDrawNode* d, bool on) {
        d->clear();
        float w = TOG_W, h = TOG_H;
        if (fxKind() == Kind::Lava) {
            CCPoint v[8];
            chamferPts(v, 0.f, 0.f, w, h, 5.f);
            d->drawPolygon(v, 8, pm(0.11f, 0.03f, 0.02f), 1.8f, on ? pm(1.f, 0.42f, 0.08f) : pm(0.55f, 0.20f, 0.10f));
            CCPoint in[8];
            chamferPts(in, 3.f, 3.f, w - 3.f, h - 3.f, 3.5f);          // magma core
            d->drawPolygon(in, 8, on ? pm(0.62f, 0.13f, 0.03f) : pm(0.25f, 0.07f, 0.04f), 0.f, pm(0.f, 0.f, 0.f, 0.f));
            ccColor4F glow = pm(1.f, 0.60f, 0.15f, on ? 0.95f : 0.35f); // glowing cracks
            d->drawSegment({ w * .18f, h * .28f }, { w * .42f, h * .42f }, 0.5f, glow);
            d->drawSegment({ w * .42f, h * .42f }, { w * .50f, h * .22f }, 0.5f, glow);
            d->drawSegment({ w * .62f, h * .80f }, { w * .84f, h * .60f }, 0.5f, glow);
            d->drawSegment({ w * .70f, h * .24f }, { w * .86f, h * .34f }, 0.5f, glow);
            d->drawSegment({ 6.f, 2.5f }, { w - 6.f, 2.5f }, 0.6f, pm(1.f, 0.5f, 0.1f, on ? 0.8f : 0.25f));
        } else {
            ccColor4F ear = pm(1.f, 0.69f, 0.63f), earIn = pm(1.f, 0.86f, 0.88f);
            CCPoint l[3] = { { 3.f, h - 2.f }, { 4.f, h + 8.f }, { 12.f, h - 2.f } };
            CCPoint r[3] = { { w - 12.f, h - 2.f }, { w - 4.f, h + 8.f }, { w - 3.f, h - 2.f } };
            d->drawPolygon(l, 3, ear, 0.f, ear);
            d->drawPolygon(r, 3, ear, 0.f, ear);
            CCPoint li[3] = { { 5.f, h - 1.f }, { 5.5f, h + 4.5f }, { 9.5f, h - 1.f } };
            CCPoint ri[3] = { { w - 9.5f, h - 1.f }, { w - 5.5f, h + 4.5f }, { w - 5.f, h - 1.f } };
            d->drawPolygon(li, 3, earIn, 0.f, earIn);
            d->drawPolygon(ri, 3, earIn, 0.f, earIn);
            CCPoint v[20];
            int n = roundRect(v, 0.f, 0.f, w, h, 7.f);
            d->drawPolygon(v, n, on ? pm(1.f, 0.84f, 0.78f) : pm(1.f, 0.94f, 0.95f), 2.f, pm(1.f, 0.69f, 0.63f));
            if (on) {                                                    // blush when ON
                d->drawDot({ 4.5f, 4.5f }, 2.2f, pm(1.f, 0.62f, 0.68f, 0.6f));
                d->drawDot({ w - 4.5f, 4.5f }, 2.2f, pm(1.f, 0.62f, 0.68f, 0.6f));
            }
        }
    }

    // the check, drawn around (0,0) so it can pop-scale about its centre
    void drawToggleCheck(CCDrawNode* d) {
        d->clear();
        float w = TOG_W, h = TOG_H;
        if (fxKind() == Kind::Lava) {
            CCPoint a(w * -.26f, h * .02f), b(w * -.07f, h * -.22f), c(w * .30f, h * .26f);
            d->drawSegment(a, b, 1.7f, pm(1.f, 0.50f, 0.06f));          // orange check
            d->drawSegment(b, c, 1.7f, pm(1.f, 0.50f, 0.06f));
            d->drawSegment(a, b, 0.6f, pm(1.f, 0.82f, 0.35f, 0.9f));    // hot core
            d->drawSegment(b, c, 0.6f, pm(1.f, 0.82f, 0.35f, 0.9f));
        } else {
            drawPawCC(d, 0.f, -0.5f, 7.5f, 0.f, pm(1.f, 0.52f, 0.58f));
        }
    }

    // ---- floating particles inside the menu: rising embers (Lava) / click paws (Cat) ----
    // One CCDrawNode, cleared and redrawn at 30 Hz. Particles are killed the moment they would cross the box,
    // and paws are pushed inside it when spawned, so nothing is ever drawn outside the menu.
    class CkyFx : public CCLayer {
    public:
        enum class Mode { Embers, Paws };
    private:
        Mode m_mode = Mode::Embers;
        cky::fx::Rect m_box{};
        CCDrawNode* m_dn = nullptr;
        cky::fx::EmberPool m_embers;
        cky::fx::PawPool m_paws;
        float m_acc = 0.f;
        bool  m_drawn = false;

    public:
        static CkyFx* create(Mode m, cky::fx::Rect box) {
            auto r = new CkyFx();
            r->m_mode = m;
            r->m_box = box;
            if (r->init()) { r->autorelease(); return r; }
            CC_SAFE_DELETE(r);
            return nullptr;
        }

        bool init() override {
            if (!CCLayer::init()) return false;
            m_dn = CCDrawNode::create();
            this->addChild(m_dn);
            this->scheduleUpdate();
            if (m_mode == Mode::Paws) this->setTouchEnabled(true);
            return true;
        }

        // never swallows: the buttons underneath still get the touch
        void registerWithTouchDispatcher() override {
            CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -700, false);
        }

        bool ccTouchBegan(CCTouch* t, CCEvent*) override {
            if (m_mode != Mode::Paws || !this->getParent()) return false;
            CCPoint p = this->getParent()->convertToNodeSpace(t->getLocation());
            if (p.x >= m_box.x0 && p.x <= m_box.x1 && p.y >= m_box.y0 && p.y <= m_box.y1) {
                m_paws.spawn(p.x, p.y, 1.f, m_box);
                m_drawn = true;
            }
            return false;
        }

        void update(float dt) override {
            m_acc += dt;
            if (m_acc < 1.f / 30.f) return;                 // 30 Hz is plenty and keeps Android cool
            float step = std::min(m_acc, 0.1f);
            m_acc = 0.f;
            if (m_mode == Mode::Embers) {
                m_embers.step(step, 1.f, m_box);
                m_dn->clear();
                for (auto const& q : m_embers.p) {
                    if (!q.alive) continue;
                    float a = cky::fx::EmberPool::alphaOf(q);
                    auto c = cky::fx::emberColor(q.age / q.life);
                    if (q.size > 1.6f) m_dn->drawDot({ q.x, q.y }, q.size * 2.2f, pmc(c, a * 0.22f)); // soft glow
                    m_dn->drawDot({ q.x, q.y }, q.size, pmc(c, a));
                }
            } else {
                if (!m_drawn) return;
                m_paws.step(step, 1.f, m_box);
                m_dn->clear();
                for (auto const& q : m_paws.p) {
                    if (!q.alive) continue;
                    drawPawCC(m_dn, q.x, q.y, q.size, q.rot, pmc(cky::fx::kPeachDeep, cky::fx::PawPool::alphaOf(q)));
                }
                m_drawn = m_paws.alive() > 0;
                if (!m_drawn) m_dn->clear();
            }
        }

        // Cat: static scattered paws, 0.4 opacity, different sizes. Drawn ONCE, zero per-frame cost.
        static CCDrawNode* makeBgPaws(float w, float h) {
            auto d = CCDrawNode::create();
            ccColor4F col = pmc(cky::fx::kPeach, cky::fx::kBgPawAlpha);
            for (auto const& b : cky::fx::kBgPaws) {
                float e = cky::fx::pawExtent(b.size) + 2.f;
                float x = std::clamp(-w / 2.f + b.nx * w, -w / 2.f + e, w / 2.f - e);
                float y = std::clamp(-h / 2.f + b.ny * h, -h / 2.f + e, h / 2.f - e);
                drawPawCC(d, x, y, b.size, b.rotDeg * PI_F / 180.f, col);
            }
            return d;
        }
    };
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
    bool    m_anim = false;         // true only while a toggle was just tapped: the check pops in
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

        Kind kind = fxKind();
        if (kind == Kind::Lava) {
            // sharp chamfered ("hexagonal") corners with a hot rim, all in one draw node
            auto bg = CCDrawNode::create();
            bg->setTag(1);
            drawLavaBtn(bg, w, h, color);
            node->addChild(bg, 0);
        } else {
            auto border = CCScale9Sprite::create("square02_001.png");
            border->setContentSize({w + 4.f, h + 4.f});
            border->setColor(kind == Kind::Cat ? ccc3(240, 160, 156) : ccc3(0, 0, 0));
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
        }

        auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
        lbl->setScale(ls);
        lbl->setPosition({w / 2.f, h / 2.f});
        lbl->setTag(2);
        node->addChild(lbl, 2);

        auto item = CCMenuItemSpriteExtra::create(node, this, cb);
        item->setTag(tag);
        item->setPosition({x, y});
        // Cat: springier press. GD's own button animation does the bounce (grow on touch, bounce back on
        // release), we just raise how far it grows. No extra actions, no cost.
        if (kind == Kind::Cat) item->m_scaleMultiplier = 1.16f;
        menu->addChild(item);
        return item;
    }

    // Cat / Lava toggle box (replaces the ON/OFF pill): a box that gets a check when ON
    CCMenuItemSpriteExtra* makeToggle(CCMenu* menu, int tag, float x, float y) {
        auto node = CCNode::create();
        node->setContentSize({TOG_W, TOG_H});
        node->setAnchorPoint({0.5f, 0.5f});

        auto bg = CCDrawNode::create();
        bg->setTag(1);
        drawToggleBox(bg, false);
        node->addChild(bg, 0);

        auto holder = CCNode::create();                    // scales about its centre
        holder->setTag(3);
        holder->setPosition({TOG_W / 2.f, TOG_H / 2.f});
        holder->setVisible(false);
        auto chk = CCDrawNode::create();
        drawToggleCheck(chk);
        holder->addChild(chk);
        node->addChild(holder, 1);

        auto item = CCMenuItemSpriteExtra::create(node, this, menu_selector(CkyPanel::onPill));
        item->setTag(tag);
        item->setPosition({x, y});
        if (fxKind() == Kind::Cat) item->m_scaleMultiplier = 1.16f;
        menu->addChild(item);
        return item;
    }

    static void paintToggle(CCMenuItemSpriteExtra* it, bool on, bool animate) {
        auto n = it ? it->getNormalImage() : nullptr;
        if (!n) return;
        auto bg = typeinfo_cast<CCDrawNode*>(n->getChildByTag(1));
        auto chk = n->getChildByTag(3);
        if (!bg || !chk || chk->isVisible() == on) return;  // nothing changed -> nothing to redraw
        drawToggleBox(bg, on);
        chk->stopAllActions();
        chk->setVisible(on);
        if (on) {
            if (animate) {
                chk->setScale(0.2f);
                chk->runAction(CCEaseBackOut::create(CCScaleTo::create(0.22f, 1.f)));
            } else {
                chk->setScale(1.f);
            }
        }
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
            auto b = n->getChildByTag(1);
            if (auto bg = typeinfo_cast<CCScale9Sprite*>(b)) bg->setColor(c);
            else if (auto d = typeinfo_cast<CCDrawNode*>(b)) {   // Lava buttons are draw nodes
                auto sz = n->getContentSize();
                drawLavaBtn(d, sz.width, sz.height, c);
            }
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
        if (fxKind() != Kind::None) { paintToggle(it, on, m_anim); return; }
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
        if (fxKind() == Kind::Cat) l->setColor(ccc3(214, 98, 124));
        m_rows->addChild(l);
    }

    void rowPill(char const* text, int tag) {
        rowLabel(text);
        if (fxKind() != Kind::None) makeToggle(m_menu, tag, 190.f, m_y);
        else makeBtn(m_menu, menu_selector(CkyPanel::onPill), tag, 176.f, m_y, 58.f, 20.f, "OFF", kOff);
        nextRow();
    }

    void rowValue(char const* text, int dn, int up, int lbl) {
        rowLabel(text);
        makeBtn(m_menu, menu_selector(CkyPanel::onBtn), dn, 120.f, m_y, 24.f, 20.f, "-", kAccent, 0.45f);
        auto v = CCLabelBMFont::create("", "bigFont.fnt");
        v->setScale(0.4f);
        v->setPosition({152.f, m_y});
        if (fxKind() == Kind::Cat) v->setColor(ccc3(214, 98, 124));
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
        l->setColor(fxKind() == Kind::Cat ? ccc3(205, 130, 150) : ccc3(150, 160, 190));
        l->setPosition({-72.f, m_y});
        l->setTag(tag);
        m_rows->addChild(l);
        nextRow(22.f);
    }

    // label + a color swatch (drawn in refresh())
    void rowSwatch(char const* text, int tag) {
        rowLabel(text);
        auto sw = CCDrawNode::create();
        sw->setTag(tag);
        sw->setPosition({176.f, m_y});
        m_rows->addChild(sw);
        nextRow();
    }

    void paintSwatch(int tag, float const* c) {
        if (!m_rows) return;
        auto sw = typeinfo_cast<CCDrawNode*>(m_rows->getChildByTag(tag));
        if (!sw) return;
        sw->clear();
        CCPoint v[4] = { {-29.f, -8.f}, {29.f, -8.f}, {29.f, 8.f}, {-29.f, 8.f} };
        sw->drawPolygon(v, 4, ccc4f(c[0], c[1], c[2], std::max(c[3], 0.15f)), 1.5f, ccc4f(0.f, 0.f, 0.f, 1.f));
    }

    void rowText(char const* text) {
        auto l = CCLabelBMFont::create(text, "bigFont.fnt");
        l->setScale(0.3f);
        l->setAnchorPoint({0.f, 0.5f});
        l->setColor(fxKind() == Kind::Cat ? ccc3(205, 130, 150) : ccc3(150, 160, 190));
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

    // ---- Molten rocks window: chamfered card + sidebar, glowing heated-metal gradient border ----
    // Static, drawn once. The border is layered OPAQUE strokes (wide+dim -> thin+bright) so it glows without
    // any alpha-overlap beads, and each stroke piece is coloured by its height: hot at the top, deep red below.
    void buildLavaChrome(float W, float H) {
        auto d = CCDrawNode::create();
        float cc = 14.f;                                       // window chamfer
        ccColor3B card = kCard, side = kSide;
        {
            CCPoint v[8];
            chamferPts(v, -W / 2.f + 1.f, -H / 2.f + 1.f, W / 2.f - 1.f, H / 2.f - 1.f, cc);
            d->drawPolygon(v, 8, pm3(card, 0.97f), 0.f, pm(0.f, 0.f, 0.f, 0.f));
        }
        {
            CCPoint v[8];
            chamferPts(v, -149.f - 59.f, -(H - 20.f) / 2.f, -149.f + 59.f, (H - 20.f) / 2.f, 8.f);
            d->drawPolygon(v, 8, pm3(side), 0.f, pm(0.f, 0.f, 0.f, 0.f));
        }

        auto top = thm().top, bot = thm().bottom;
        auto colAt = [&](float y, float mix) {
            float t = std::clamp((y + H / 2.f) / H, 0.f, 1.f);
            float r = (bot.r + (top.r - bot.r) * t) / 255.f;
            float g = (bot.g + (top.g - bot.g) * t) / 255.f;
            float b = (bot.b + (top.b - bot.b) * t) / 255.f;
            return ccc4f(card.r / 255.f + (r - card.r / 255.f) * mix,
                         card.g / 255.f + (g - card.g / 255.f) * mix,
                         card.b / 255.f + (b - card.b / 255.f) * mix, 1.f);
        };
        CCPoint p[8];
        chamferPts(p, -W / 2.f + 5.f, -H / 2.f + 5.f, W / 2.f - 5.f, H / 2.f - 5.f, cc - 3.f);
        auto stroke = [&](float radius, float mix) {
            for (int i = 0; i < 8; ++i) {
                CCPoint a = p[i], b = p[(i + 1) % 8];
                float len = ccpDistance(a, b);
                int n = std::max(1, static_cast<int>(std::ceil(len / 24.f)));
                for (int k = 0; k < n; ++k) {
                    CCPoint s0 = ccpLerp(a, b, static_cast<float>(k) / n);
                    CCPoint s1 = ccpLerp(a, b, static_cast<float>(k + 1) / n);
                    d->drawSegment(s0, s1, radius, colAt((s0.y + s1.y) * 0.5f, mix));
                }
            }
        };
        stroke(5.f, 0.22f);     // outer glow
        stroke(3.6f, 0.50f);
        stroke(2.2f, 0.80f);
        stroke(1.0f, 1.00f);    // white-hot core line
        this->addChild(d, 1);
    }

    // ---- build ----
    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-panel");
        cky::theme = Mod::get()->getSavedValue<int>("theme", 0);
        float W = 440.f, H = 270.f;

        Kind kind = fxKind();
        // the box particles live in (card interior, a few px in so chamfered corners stay clear)
        cky::fx::Rect box{ -(W - 12.f) / 2.f + 4.f, -(H - 12.f) / 2.f + 4.f,
                            (W - 12.f) / 2.f - 4.f,  (H - 12.f) / 2.f - 4.f };

        if (kind == Kind::Lava) {
            buildLavaChrome(W, H);
        } else {
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
        }

        if (kind == Kind::Cat) {
            this->addChild(CkyFx::makeBgPaws(W - 12.f, H - 12.f), 2);               // scattered paws, 0.4 opacity
            this->addChild(CkyFx::create(CkyFx::Mode::Paws, box), 6);               // click paws, on top
        } else if (kind == Kind::Lava) {
            this->addChild(CkyFx::create(CkyFx::Mode::Embers, box), 2);             // rising embers, behind the widgets
        }

        auto title = CCLabelBMFont::create("ChxrCky", "bigFont.fnt");
        title->setScale(0.55f);
        title->setColor(thm().title);
        title->setPosition({-149.f, 108.f});
        this->addChild(title, 3);

        auto sub = CCLabelBMFont::create("Macro Bot", "bigFont.fnt");
        sub->setScale(0.3f);
        sub->setColor(fxKind() == Kind::Cat ? ccc3(210, 140, 160) : ccc3(150, 160, 190));
        sub->setPosition({-149.f, 92.f});
        this->addChild(sub, 3);

        m_tabMenu = CCMenu::create();
        m_tabMenu->setPosition({0.f, 0.f});
        this->addChild(m_tabMenu, 4);
        static char const* names[7] = { "Bot", "Global", "Player", "Display", "Labels", "Clicks", "Settings" };
        static int const pages[7]   = { 0, 1, 2, 3, PAGE_LABELS, 4, 5 };
        for (int i = 0; i < 7; ++i) {
            makeBtn(m_tabMenu, menu_selector(CkyPanel::onTab), TAB_BASE + pages[i],
                    -149.f, 64.f - 27.f * static_cast<float>(i), 104.f, 24.f, names[i], kTab, 0.4f);
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

        static char const* names[9] = { "Bot", "Global", "Player", "Display", "Clicks", "Settings",
                                        "HUD settings", "Hitbox trail", "Labels" };
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
            rowPill("Practice fixes", T_PFIX);
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
            break;
        case 3:
            rowPill("Hitboxes", T_HITBOX);
            {
                float ty = m_y;
                rowPill("Hitbox trail", T_TRAIL);
                makeGear(m_menu, B_TR_GEAR, 122.f, ty);
            }
            {
                float hy = m_y;
                rowPill("HUD", T_HUD);
                makeGear(m_menu, B_HUDGEAR, 122.f, hy);
            }
            rowValue("HUD size", B_HUDSZ_DN, B_HUDSZ_UP, L_HUDSZ);
            rowPill("StartPos switcher", T_STARTPOS);
            rowPill("Input overlay", T_OVERLAY);
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
        case 7: {
            rowValue("Trail length", B_TRL_DN, B_TRL_UP, L_TRL);
            static char const* chn[4] = { "  Red", "  Green", "  Blue", "  Opacity" };
            for (int which = 0; which < 2; ++which) {
                rowSwatch(which == 0 ? "Outer hitbox" : "Inner hitbox", which == 0 ? L_SW_OUT : L_SW_IN);
                for (int ch = 0; ch < 4; ++ch) {
                    int base = B_TRC_BASE + which * 8 + ch * 2;
                    rowValue(chn[ch], base, base + 1, L_TRC_BASE + which * 4 + ch);
                }
            }
            rowWide("Back to Display", B_TR_BACK);
            break;
        }
        case PAGE_LABELS:
            rowPill("CPS", T_CLICKHUD);
            rowPill("FPS", T_LBL_FPS);
            rowPill("Noclip accuracy", T_NCACC);
            rowPill("Noclip deaths", T_NCDEATH);
            rowPill("Startpos run", T_LBL_RUN);
            rowPill("Level time", T_LBL_TIME);
            rowPill("Best run", T_LBL_BEST);
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
        int active = (m_tab == 6 || m_tab == 7) ? 3 : m_tab; // HUD settings / Hitbox trail belong to Display
        for (int i = 0; i <= PAGE_LABELS; ++i) {
            auto it = typeinfo_cast<CCMenuItemSpriteExtra*>(m_tabMenu->getChildByTag(TAB_BASE + i));
            paint(it, i == active ? kAccent : kTab);
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
        setPill(T_TRAIL, cky::hitboxTrail);
        setPill(T_HUD, cky::hudOn);
        setPill(T_STARTPOS, cky::startPosOn);
        setPill(T_TELEM, cky::ckptTelemetry);
        setPill(T_OVERLAY, cky::overlayOn);
        setPill(T_CLICKHUD, cky::clickHudOn);
        setPill(T_CLICKBOT, cky::clickbotOn);
        setPill(T_TRAJ, cky::trajectory);
        setPill(T_FFIX, cky::frameFixes);
        setPill(T_PFIX, cky::practiceFixes);
        setPill(T_LBL_FPS, cky::lblFps);
        setPill(T_LBL_RUN, cky::lblRun);
        setPill(T_LBL_TIME, cky::lblTime);
        setPill(T_LBL_BEST, cky::lblBest);
        setPill(T_HUDSTEP, cky::hudStepShow);
        setPill(T_HUDSPEED, cky::hudSpeedShow);
        setPill(T_BACKSTEP, cky::backStepOn);
        setPill(T_STEPBTNS, cky::stepperBtns);
        setPill(T_SPEEDBTNS, cky::speedBtns);
        setPill(T_BACKALWAYS, cky::backAlways);
        setText(L_TLEN, fmt::format("{}", cky::trajLen));
        setText(L_TRL, fmt::format("{}", cky::trailLen));
        for (int which = 0; which < 2; ++which) {
            float const* c = which == 0 ? cky::trailOuter : cky::trailInner;
            for (int ch = 0; ch < 4; ++ch) {
                setText(L_TRC_BASE + which * 4 + ch,
                    fmt::format("{}", static_cast<int>(std::lround(c[ch] * 255.f))));
            }
            paintSwatch(which == 0 ? L_SW_OUT : L_SW_IN, c);
        }
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
        cky::makeMacroFolders();
        setText(L_THEME, thm().name);
        // long names (e.g. "Molten rocks") shrink so they still fit between the +/- buttons
        if (m_rows) {
            if (auto l = typeinfo_cast<CCLabelBMFont*>(m_rows->getChildByTag(L_THEME))) {
                size_t len = std::strlen(thm().name);
                l->setScale(len > 8 ? 0.4f * 8.f / static_cast<float>(len) : 0.4f);
            }
        }

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
        case T_TRAIL:
            cky::hitboxTrail = !cky::hitboxTrail;
            ckyTrailApply();
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
        case T_PFIX:
            if (cky::recording) { ckyNotify("Stop recording first", false); break; }
            cky::practiceFixes = !cky::practiceFixes;
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
        case T_LBL_FPS:
            cky::lblFps = !cky::lblFps;
            break;
        case T_LBL_RUN:
            cky::lblRun = !cky::lblRun;
            break;
        case T_LBL_TIME:
            cky::lblTime = !cky::lblTime;
            break;
        case T_LBL_BEST:
            cky::lblBest = !cky::lblBest;
            break;
        case T_BACKALWAYS:
            cky::backAlways = !cky::backAlways;
            if (!cky::backAlways && !cky::frozen) ckyBackStepClear();
            break;
        default: break;
        }
        cky::labelsSave(); // label switches are remembered between launches
        m_anim = true;     // the check pops in only for a real tap, not when a tab is (re)built
        refresh();
        m_anim = false;
        ckyHudRefresh();
    }

    void onBtn(CCObject* s) {
        int t = static_cast<CCNode*>(s)->getTag();
        if (t >= B_TRC_BASE && t < B_TRC_BASE + 16) {
            // hitbox trail color: R/G/B/opacity of the outer or inner hitbox, steps of 15/255
            int i = t - B_TRC_BASE;
            float* c = (i / 8 == 0) ? cky::trailOuter : cky::trailInner;
            int ch = (i % 8) / 2;
            int v = static_cast<int>(std::lround(c[ch] * 255.f)) + ((i % 2) ? 15 : -15);
            c[ch] = static_cast<float>(std::clamp(v, 0, 255)) / 255.f;
            ckyTrailSave();
            refresh();
            return;
        }
        switch (t) {
        case B_TRL_DN:
        case B_TRL_UP:
            cky::trailLen = std::clamp(cky::trailLen + (t == B_TRL_UP ? 20 : -20), 20, 1200);
            ckyTrailSave();
            break;
        case B_TR_GEAR:
            buildTab(7);
            return;
        case B_TR_BACK:
            buildTab(3);
            return;
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
            int n = cky::ThemeCount;
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
