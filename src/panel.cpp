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
        B_HUDGEAR, B_HUDBACK, B_FI_DN, B_FI_UP, B_FVEL, B_HUDOP_DN, B_HUDOP_UP,
        L_TPS = 70, L_TSTEP, L_SPEED, L_HUDSZ, L_ICONSZ, L_SLOT, L_INFO, L_INFO2, L_INFO3,
        L_PACK, L_THEME, L_OFF, L_VOL, L_TLEN, L_FI, L_HUDOP,
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

    // row label of a toggle row = LBL_TAGS + the toggle's tag (Cyber tints it cyan / grey)
    enum : int { LBL_TAGS = 1000 };

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
                out[n++] = CCPoint(cx[c] + std::cos(a) * r, cy[c] + std::sin(a) * r);
            }
        }
        return n;
    }

    // chamfered ("hexagonal") corners: 8 points
    void chamferPts(CCPoint* o, float x0, float y0, float x1, float y1, float c) {
        float f[8][2];
        cky::fx::chamfer(x0, y0, x1, y1, c, f);
        for (int i = 0; i < 8; ++i) o[i] = CCPoint(f[i][0], f[i][1]);
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

    // Cyber button: sharp chamfered corners, glowing cyan outline (soft halo first, crisp line on top)
    void drawCyberBtn(CCDrawNode* d, float w, float h, ccColor3B c) {
        d->clear();
        float k = std::min(h * 0.3f, 6.f);
        CCPoint v[8];
        chamferPts(v, 0.f, 0.f, w, h, k);
        d->drawPolygon(v, 8, pm(0.f, 0.f, 0.f, 0.f), 3.6f, pm(0.f, 0.9f, 1.f, 0.20f));
        d->drawPolygon(v, 8, pm3(c), 1.3f, pm(0.25f, 0.95f, 1.f, 1.f));
        d->drawSegment({ k + 1.f, h - 1.f }, { w - k - 1.f, h - 1.f }, 0.4f, pm(0.7f, 1.f, 1.f, 0.35f));
    }

    // Matrix button: SOLID black box (c is black, or a very dark green for the selected tab / +/-), green border,
    // one red "corrupted" tick in the corner
    void drawMatrixBtn(CCDrawNode* d, float w, float h, ccColor3B c) {
        d->clear();
        CCPoint v[4] = { { 0.f, 0.f }, { w, 0.f }, { w, h }, { 0.f, h } };
        d->drawPolygon(v, 4, pm3(c), 1.4f, pm(0.f, 1.f, 0.35f, 0.95f));
        d->drawSegment({ w - 10.f, h - 1.f }, { w - 3.f, h - 1.f }, 0.5f, pm(1.f, 0.2f, 0.2f, 0.9f));
    }

    // buttons that are one CCDrawNode (instead of the stock bordered sprite) in these themes
    bool drawnBtn(Kind k) { return k == Kind::Lava || k == Kind::Cyber || k == Kind::Matrix; }
    // The +/- buttons and the selected tab use the theme's bright accent colour. In these three themes the label is
    // light, so a bright fill would swallow it: those buttons get a dark version of the accent instead.
    bool isAccent(ccColor3B c) {
        auto const& a = thm().accent;
        return c.r == a.r && c.g == a.g && c.b == a.b;
    }
    void drawKindBtn(CCDrawNode* d, float w, float h, ccColor3B c) {
        bool hot = isAccent(c);
        switch (fxKind()) {
            case Kind::Cyber:  drawCyberBtn(d, w, h, hot ? ccc3(0, 62, 96) : c); break;
            case Kind::Matrix: drawMatrixBtn(d, w, h, hot ? ccc3(0, 46, 14) : ccc3(0, 0, 0)); break;   // solid black
            default:           drawLavaBtn(d, w, h, hot ? ccc3(150, 52, 12) : c); break;
        }
    }

    // text colours per theme (labels / small grey text)
    // All of these are picked for strong contrast against the (always opaque) card of their theme.
    ccColor3B labelCol() {
        switch (fxKind()) {
            case Kind::Cat:    return ccc3(150, 40, 78);     // deep rose on the near-white pink card
            case Kind::Matrix: return ccc3(200, 255, 215);
            case Kind::Cyber:  return ccc3(190, 242, 250);
            default:           return ccc3(255, 255, 255);   // Molten rocks: pure white
        }
    }
    ccColor3B subCol() {
        switch (fxKind()) {
            case Kind::Cat:    return ccc3(170, 84, 112);
            case Kind::Matrix: return ccc3(70, 200, 105);
            case Kind::Cyber:  return ccc3(140, 190, 210);
            case Kind::Lava:   return ccc3(255, 226, 90);    // Molten rocks: bright yellow
            default:           return ccc3(150, 160, 190);
        }
    }
    ccColor3B titleCol() {
        switch (fxKind()) {
            case Kind::Cat:  return ccc3(168, 52, 90);
            case Kind::Lava: return ccc3(255, 232, 90);
            default:         return thm().title;
        }
    }
    // text on a button. Cat's buttons are light peach, so their text is dark rose instead of white.
    ccColor3B btnTextCol() {
        switch (fxKind()) {
            case Kind::Cat:    return ccc3(120, 36, 64);
            case Kind::Matrix: return ccc3(200, 255, 215);
            case Kind::Cyber:  return ccc3(190, 250, 255);
            default:           return ccc3(255, 255, 255);
        }
    }
    const ccColor3B kCyberRowOff = ccc3(140, 156, 176);      // Cyber toggle row, switch OFF (was a dark grey)

    // ---- toggle boxes (26x22). Cat = cat-face box with ears + paw check. Lava = magma box + orange check.
    //      Cyber = hexagonal switch (knob slides, neon glow ring pulses when ON). Matrix = binary text block. ----
    constexpr float TOG_W = 26.f, TOG_H = 22.f;
    constexpr float MX_W = 46.f, MX_H = 20.f;      // Matrix toggle (fits "0 X 0")

    void drawToggleBox(CCDrawNode* d, bool on) {
        d->clear();
        bool mx = fxKind() == Kind::Matrix;
        float w = mx ? MX_W : TOG_W, h = mx ? MX_H : TOG_H;
        if (mx) {
            // no checkbox: a solid black block, green binary when ON and red + "corrupted" when OFF
            CCPoint v[4] = { { 0.f, 0.f }, { w, 0.f }, { w, h }, { 0.f, h } };
            if (on) {
                d->drawPolygon(v, 4, pm(0.f, 0.f, 0.f), 1.2f, pm(0.f, 0.95f, 0.30f, 0.9f));
            } else {
                d->drawPolygon(v, 4, pm(0.f, 0.f, 0.f), 1.2f, pm(1.f, 0.2f, 0.2f, 0.9f));
                for (int i = 0; i < 2; ++i) {                       // random torn strips, drawn once per state change
                    float y = cky::fx::rf(3.f, h - 3.f), x = cky::fx::rf(2.f, w - 14.f);
                    d->drawSegment({ x, y }, { x + cky::fx::rf(6.f, 12.f), y }, 0.5f, pm(1.f, 0.35f, 0.35f, 0.8f));
                }
            }
        } else if (fxKind() == Kind::Cyber) {
            float c = 6.f;
            CCPoint v[6] = { { c, 0.f }, { w - c, 0.f }, { w, h / 2.f }, { w - c, h }, { c, h }, { 0.f, h / 2.f } };
            d->drawPolygon(v, 6, on ? pm(0.01f, 0.14f, 0.20f) : pm(0.07f, 0.08f, 0.10f), 1.6f,
                           on ? pm(0.f, 0.95f, 1.f) : pm(0.22f, 0.24f, 0.28f));
            float kx = on ? w - 9.f : 9.f, cy = h / 2.f;               // knob: left = OFF, right = ON
            CCPoint k[6] = { { kx - 3.f, cy - 4.f }, { kx + 3.f, cy - 4.f }, { kx + 5.f, cy },
                             { kx + 3.f, cy + 4.f }, { kx - 3.f, cy + 4.f }, { kx - 5.f, cy } };
            d->drawPolygon(k, 6, on ? pm(0.65f, 1.f, 1.f) : pm(0.25f, 0.27f, 0.31f), 0.f, pm(0.f, 0.f, 0.f, 0.f));
        } else if (fxKind() == Kind::Lava) {
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
        if (fxKind() == Kind::Cyber) {
            // neon halo + inner glow of the ON switch. The holder node breathes (scale action), nothing is redrawn.
            float c = 6.f, hw = w / 2.f + 1.5f, hh = h / 2.f + 1.5f;
            CCPoint v[6] = { { -hw + c, -hh }, { hw - c, -hh }, { hw, 0.f }, { hw - c, hh }, { -hw + c, hh }, { -hw, 0.f } };
            d->drawPolygon(v, 6, pm(0.f, 0.9f, 1.f, 0.12f), 3.4f, pm(0.f, 0.95f, 1.f, 0.38f));
        } else if (fxKind() == Kind::Lava) {
            CCPoint a(w * -.26f, h * .02f), b(w * -.07f, h * -.22f), c(w * .30f, h * .26f);
            d->drawSegment(a, b, 1.7f, pm(1.f, 0.50f, 0.06f));          // orange check
            d->drawSegment(b, c, 1.7f, pm(1.f, 0.50f, 0.06f));
            d->drawSegment(a, b, 0.6f, pm(1.f, 0.82f, 0.35f, 0.9f));    // hot core
            d->drawSegment(b, c, 0.6f, pm(1.f, 0.82f, 0.35f, 0.9f));
        } else {
            drawPawCC(d, 0.f, -0.5f, 7.5f, 0.f, pm(1.f, 0.52f, 0.58f));
        }
    }

    // ---- Matrix toggle click: the text scrolls random characters, then locks in left to right (~0.5 s) ----
    // A tiny node that lives NEXT TO the label (CCLabelBMFont is a sprite batch node: it only takes sprites as
    // children). It is only scheduled while a scramble runs, so an idle toggle costs nothing.
    class CkyScramble : public CCNode {
        CCLabelBMFont* m_lbl = nullptr;     // sibling, same parent: they are always freed together
        char m_text[16] = {};
        ccColor3B m_col = { 255, 255, 255 };
        float m_t = 0.f, m_acc = 0.f;
        uint32_t m_salt = 0;
    public:
        static constexpr int TAG = 777;
        static void start(CCNode* holder, CCLabelBMFont* lbl, char const* text, ccColor3B col) {
            if (!holder || !lbl) return;
            auto s = static_cast<CkyScramble*>(holder->getChildByTag(TAG));   // only this class ever uses TAG
            if (!s) {
                s = new CkyScramble();
                if (!s->init()) { CC_SAFE_DELETE(s); lbl->setString(text); lbl->setColor(col); return; }
                s->autorelease();
                s->setTag(TAG);
                holder->addChild(s);
            }
            s->m_lbl = lbl;
            std::snprintf(s->m_text, sizeof(s->m_text), "%s", text);
            s->m_col = col;
            s->m_t = 0.f;
            s->m_acc = 1.f;                                    // draw the first frame right away
            s->m_salt = cky::fx::rnd();
            lbl->setColor(ccc3(200, 255, 215));                // scrolling characters show pale, the final colour locks in
            s->scheduleUpdate();
        }
        static void cancel(CCNode* holder) {
            if (!holder) return;
            if (auto s = static_cast<CkyScramble*>(holder->getChildByTag(TAG))) s->unscheduleUpdate();
        }
        void update(float dt) override {
            if (!m_lbl) { this->unscheduleUpdate(); return; }
            m_t += dt;
            m_acc += dt;
            if (m_acc < 1.f / 30.f) return;                    // 30 steps a second is a readable scroll
            m_acc = 0.f;
            char buf[16];
            uint32_t epoch = static_cast<uint32_t>(m_t * 30.f) + m_salt;
            bool done = cky::fx::scramble(m_text, m_t, epoch, buf, sizeof(buf));
            m_lbl->setString(buf);
            if (done) { m_lbl->setColor(m_col); this->unscheduleUpdate(); }
        }
    };

    // ---- floating particles inside the menu: rising embers (Lava) / click paws (Cat) / digital rain (Matrix) ----
    // One CCDrawNode, cleared and redrawn at 30 Hz. Particles are killed the moment they would cross the box,
    // and paws are pushed inside it when spawned, so nothing is ever drawn outside the menu.
    class CkyFx : public CCLayer {
    public:
        enum class Mode { Embers, Paws, Rain };
    private:
        Mode m_mode = Mode::Embers;
        cky::fx::Rect m_box{};
        CCDrawNode* m_dn = nullptr;
        cky::fx::EmberPool m_embers;
        cky::fx::PawPool m_paws;
        cky::fx::RainSim m_rain;
        static constexpr float RAIN_CELL = 9.f;               // Cocos px per rain cell
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
            // 30 Hz is plenty and keeps Android cool; the rain is faint, so 20 Hz
            if (m_acc < (m_mode == Mode::Rain ? 1.f / 20.f : 1.f / 30.f)) return;
            float step = std::min(m_acc, 0.1f);
            m_acc = 0.f;
            if (m_mode == Mode::Rain) {
                // Faint green rain at 0.2 opacity: columns of tiny "1" bars and "0" boxes. One draw node, cleared and
                // redrawn at 20 Hz (24 columns x 7 cells, ~170 shapes). A cell is skipped unless it is fully inside the card.
                m_rain.step(step, RAIN_CELL, m_box);
                m_dn->clear();
                float hh = RAIN_CELL * 0.38f, ww = RAIN_CELL * 0.22f;
                for (int i = 0; i < m_rain.n; ++i) {
                    float x = m_rain.colX(i, m_box);
                    if (x - ww < m_box.x0 || x + ww > m_box.x1) continue;
                    for (int k = 0; k < cky::fx::RainSim::TRAIL; ++k) {
                        float y = m_rain.c[i].head + static_cast<float>(k) * RAIN_CELL;
                        if (y - hh < m_box.y0 || y + hh > m_box.y1) continue;
                        float a = cky::fx::RainSim::cellAlpha(k) * cky::fx::kRainAlpha;
                        ccColor4F col = k == 0 ? pm(0.75f, 1.f, 0.8f, a) : pm(0.f, 1.f, 0.35f, a);
                        if (m_rain.bit(i, k)) {
                            m_dn->drawSegment({ x, y - hh }, { x, y + hh }, 0.7f, col);              // "1"
                        } else {
                            CCPoint q[4] = { { x - ww, y - hh }, { x + ww, y - hh }, { x + ww, y + hh }, { x - ww, y + hh } };
                            m_dn->drawPolygon(q, 4, pm(0.f, 0.f, 0.f, 0.f), 0.7f, col);               // "0"
                        }
                    }
                }
            } else if (m_mode == Mode::Embers) {
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

        // Cyber: faint glowing grid. Drawn ONCE (34 lines + a few glow dots), zero per-frame cost.
        static CCDrawNode* makeGrid(float w, float h) {
            auto d = CCDrawNode::create();
            constexpr float STEP = 20.f;
            int ix = 0;
            for (float x = -w / 2.f + STEP; x < w / 2.f - 1.f; x += STEP, ++ix) {
                bool major = (ix % 4) == 3;
                d->drawSegment({ x, -h / 2.f + 1.f }, { x, h / 2.f - 1.f }, major ? 0.55f : 0.3f,
                               pm(0.f, 0.85f, 1.f, major ? 0.09f : 0.04f));
            }
            int iy = 0;
            for (float y = -h / 2.f + STEP; y < h / 2.f - 1.f; y += STEP, ++iy) {
                bool major = (iy % 4) == 3;
                d->drawSegment({ -w / 2.f + 1.f, y }, { w / 2.f - 1.f, y }, major ? 0.55f : 0.3f,
                               pm(0.f, 0.85f, 1.f, major ? 0.09f : 0.04f));
            }
            ix = 0;
            for (float x = -w / 2.f + STEP; x < w / 2.f - 1.f; x += STEP, ++ix) {
                if ((ix % 4) != 3) continue;
                iy = 0;
                for (float y = -h / 2.f + STEP; y < h / 2.f - 1.f; y += STEP, ++iy)
                    if ((iy % 4) == 3) d->drawDot({ x, y }, 1.5f, pm(0.f, 0.9f, 1.f, 0.14f));   // glowing intersections
            }
            return d;
        }

        // Cyber: the scanning laser. Drawn ONCE (a core + soft bands, none overlapping) and then just moved up and
        // down by a repeating action: no redraw, no update(). It sits BEHIND the widgets and the text (z = 1), so it
        // can never make a label hard to read. w = card width, h = window height. The bands stay inside the card.
        static CCDrawNode* makeLaser(float w, float h) {
            auto d = CCDrawNode::create();
            float half = w / 2.f - 3.f;
            for (int i = 0; i < 4; ++i) {                       // soft bands, thicker than before, never overlapping
                float off = 2.f + static_cast<float>(i) * 2.2f;
                float a = 0.10f * (1.f - static_cast<float>(i) / 4.f);
                d->drawSegment({ -half, off },  { half, off },  1.8f, pm(0.f, 1.f, 1.f, a));
                d->drawSegment({ -half, -off }, { half, -off }, 1.8f, pm(0.f, 1.f, 1.f, a));
            }
            d->drawSegment({ -half, 0.f }, { half, 0.f }, 1.6f, pm(0.5f, 1.f, 1.f, 0.70f));
            float travel = h / 2.f - 12.f;                      // top edge to bottom edge of the window (bands stay inside it)
            d->setPosition({ 0.f, -travel });
            d->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCMoveTo::create(3.8f, ccp(0.f, travel))),
                CCEaseSineInOut::create(CCMoveTo::create(3.8f, ccp(0.f, -travel))), nullptr)));
            return d;
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
    // Matrix: one label at a time gets a character swapped to a red / white symbol for a split second
    CCLabelBMFont* m_glLbl = nullptr;
    std::string    m_glOrig, m_glShown;
    int            m_glIdx = -1;
    float          m_glLeft = 0.f, m_glAcc = 0.f;

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
        if (drawnBtn(kind)) {
            // Lava: chamfered corners + hot rim. Cyber: chamfered + glowing cyan outline. Matrix: flat black + green
            // border. All in one draw node (tag 1), repainted by paint()
            auto bg = CCDrawNode::create();
            bg->setTag(1);
            drawKindBtn(bg, w, h, color);
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
        if (kind != Kind::None) lbl->setColor(btnTextCol());
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
        bool mx = fxKind() == Kind::Matrix;
        float tw = mx ? MX_W : TOG_W, th = mx ? MX_H : TOG_H;
        auto node = CCNode::create();
        node->setContentSize({tw, th});
        node->setAnchorPoint({0.5f, 0.5f});

        auto bg = CCDrawNode::create();
        bg->setTag(1);
        drawToggleBox(bg, false);
        node->addChild(bg, 0);

        auto holder = CCNode::create();                    // scales about its centre
        holder->setTag(3);                                 // (its visibility is also "is this toggle ON")
        holder->setPosition({tw / 2.f, th / 2.f});
        holder->setVisible(false);
        if (mx) {
            // Matrix: no check. The block shows binary text: "1 0 1" green = ON, "0 X 0" red = OFF
            auto bin = CCLabelBMFont::create(cky::fx::kBinOff, "bigFont.fnt");
            bin->setScale(0.34f);
            bin->setColor(ccc3(255, 50, 50));
            bin->setPosition({tw / 2.f, th / 2.f});
            bin->setTag(5);
            node->addChild(bin, 1);
        } else {
            auto chk = CCDrawNode::create();
            drawToggleCheck(chk);
            holder->addChild(chk);
        }
        node->addChild(holder, 1);

        auto item = CCMenuItemSpriteExtra::create(node, this, menu_selector(CkyPanel::onPill));
        item->setTag(tag);
        item->setPosition({x, y});
        if (fxKind() == Kind::Cat) item->m_scaleMultiplier = 1.16f;
        menu->addChild(item);
        return item;
    }

    // returns true when the toggle actually changed state (so callers can react only to a real change)
    static bool paintToggle(CCMenuItemSpriteExtra* it, bool on, bool animate) {
        auto n = it ? it->getNormalImage() : nullptr;
        if (!n) return false;
        auto bg = typeinfo_cast<CCDrawNode*>(n->getChildByTag(1));
        auto chk = n->getChildByTag(3);
        if (!bg || !chk || chk->isVisible() == on) return false;  // nothing changed -> nothing to redraw
        drawToggleBox(bg, on);
        chk->stopAllActions();
        chk->setVisible(on);
        if (fxKind() == Kind::Matrix) {
            auto lbl = typeinfo_cast<CCLabelBMFont*>(n->getChildByTag(5));
            if (!lbl) return true;
            char const* txt = on ? cky::fx::kBinOn : cky::fx::kBinOff;
            ccColor3B col = on ? ccc3(0, 255, 80) : ccc3(255, 50, 50);
            if (animate) {
                CkyScramble::start(n, lbl, txt, col);       // glitch through random characters, then lock in
            } else {
                CkyScramble::cancel(n);
                lbl->setString(txt);
                lbl->setColor(col);
            }
            return true;
        }
        if (on) {
            if (animate) {
                chk->setScale(0.2f);
                chk->runAction(CCEaseBackOut::create(CCScaleTo::create(0.22f, 1.f)));
            } else {
                chk->setScale(1.f);
            }
            if (fxKind() == Kind::Cyber) {                 // neon ring breathes while ON: one scale action, no redraw
                float t = 0.55f + 0.2f * static_cast<float>(cky::fx::rnd() & 3u) / 3.f;   // desync the switches a little
                chk->runAction(CCRepeatForever::create(CCSequence::create(
                    CCDelayTime::create(animate ? 0.25f : 0.f),
                    CCEaseSineInOut::create(CCScaleTo::create(t, 1.10f)),
                    CCEaseSineInOut::create(CCScaleTo::create(t, 0.96f)), nullptr)));
            }
        }
        return true;
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
            else if (auto d = typeinfo_cast<CCDrawNode*>(b)) {   // Lava / Cyber / Matrix buttons are draw nodes
                auto sz = n->getContentSize();
                drawKindBtn(d, sz.width, sz.height, c);
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
        if (fxKind() != Kind::None) {
            bool changed = paintToggle(it, on, m_anim);
            if (changed && fxKind() == Kind::Cyber) tintRowLabel(tag, on, m_anim);
            return;
        }
        retext(it, on ? "ON" : "OFF");
        paint(it, on ? kOn : kOff);
    }

    // Cyber: the row's text is bright neon cyan when ON and dark grey when OFF. A real tap "boots it up":
    // it fades from dark to bright. One tint action per tap, nothing per frame.
    void tintRowLabel(int tag, bool on, bool boot) {
        auto l = typeinfo_cast<CCLabelBMFont*>(m_rows ? m_rows->getChildByTag(LBL_TAGS + tag) : nullptr);
        if (!l) return;
        l->stopAllActions();
        if (on && boot) {
            l->setColor(ccc3(90, 130, 146));
            l->runAction(CCTintTo::create(0.5f, 0, 240, 255));
        } else {
            l->setColor(on ? ccc3(0, 240, 255) : kCyberRowOff);
        }
    }

    // ---- Matrix: label glitch (one label at a time, ~0.1 s, about twice a second) ----
    void glitchRestore() {
        if (m_glLbl) {
            if (m_glShown == m_glLbl->getString()) m_glLbl->setString(m_glOrig.c_str());   // nobody changed it meanwhile
            if (auto ch = typeinfo_cast<CCSprite*>(m_glLbl->getChildByTag(m_glIdx))) ch->setColor(ccc3(255, 255, 255));
        }
        m_glLbl = nullptr;
        m_glLeft = 0.f;
    }

    void glitchStart() {
        if (!m_rows) return;
        auto kids = m_rows->getChildren();
        if (!kids) return;
        CCLabelBMFont* cand[48];
        int n = 0;
        for (auto c : CCArrayExt<CCNode*>(kids)) {
            auto l = typeinfo_cast<CCLabelBMFont*>(c);
            if (l && l->isVisible() && n < 48) cand[n++] = l;
        }
        if (n == 0) return;
        auto l = cand[cky::fx::rnd() % static_cast<uint32_t>(n)];
        std::string s = l->getString();
        if (s.empty()) return;
        size_t i = cky::fx::rnd() % s.size();
        if (s[i] == ' ') return;
        m_glLbl = l;
        m_glOrig = s;
        m_glIdx = static_cast<int>(i);
        s[i] = cky::fx::kScrambleSet[cky::fx::rnd() % (sizeof(cky::fx::kScrambleSet) - 1)];
        m_glShown = s;
        l->setString(s.c_str());
        if (auto ch = typeinfo_cast<CCSprite*>(l->getChildByTag(m_glIdx)))
            ch->setColor((cky::fx::rnd() & 1u) ? ccc3(255, 60, 60) : ccc3(255, 255, 255));
        m_glLeft = cky::fx::rf(0.07f, 0.16f);
    }

    void update(float dt) override {
        if (fxKind() != Kind::Matrix) return;
        if (m_glLbl) {
            m_glLeft -= dt;
            if (m_glLeft <= 0.f) glitchRestore();
            return;
        }
        m_glAcc += dt;
        if (m_glAcc >= 0.35f) {
            m_glAcc = 0.f;
            if (cky::fx::rnd() % 100 < 70) glitchStart();
        }
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

    CCLabelBMFont* rowLabel(char const* text) {
        auto l = CCLabelBMFont::create(text, "bigFont.fnt");
        l->setScale(0.4f);
        l->setAnchorPoint({0.f, 0.5f});
        l->setPosition({-72.f, m_y});
        l->setColor(labelCol());
        m_rows->addChild(l);
        return l;
    }

    void rowPill(char const* text, int tag) {
        auto lbl = rowLabel(text);
        if (fxKind() == Kind::Cyber) {                    // toggle rows: dark grey until switched ON
            lbl->setTag(LBL_TAGS + tag);
            lbl->setColor(kCyberRowOff);
        }
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
        v->setColor(labelCol());
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
        l->setColor(subCol());
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
        l->setColor(subCol());
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
            d->drawPolygon(v, 8, pm3(card, 1.f), 0.f, pm(0.f, 0.f, 0.f, 0.f));   // fully opaque
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
            card->setOpacity(255);   // menus are always fully opaque
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
        } else if (kind == Kind::Matrix) {
            this->addChild(CkyFx::create(CkyFx::Mode::Rain, box), 2);               // faint digital rain, behind the widgets
            this->scheduleUpdate();                                                 // label glitch ticker
        } else if (kind == Kind::Cyber) {
            this->addChild(CkyFx::makeGrid(W - 12.f, H - 12.f), 1);                 // glowing grid (above the card, under the sidebar)
            this->addChild(CkyFx::makeLaser(W - 12.f, H), 1);                       // scanning laser, BEHIND the widgets and text
        }

        auto title = CCLabelBMFont::create("ChxrCky", "bigFont.fnt");
        title->setScale(0.55f);
        title->setColor(titleCol());
        title->setPosition({-149.f, 108.f});
        this->addChild(title, 3);

        auto sub = CCLabelBMFont::create("Macro Bot", "bigFont.fnt");
        sub->setScale(0.3f);
        sub->setColor(subCol());
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
        m_glLbl = nullptr;                        // its row is about to be freed
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
        head->setColor(titleCol());
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
            rowValue("HUD opacity", B_HUDOP_DN, B_HUDOP_UP, L_HUDOP);
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
        setText(L_HUDOP, fmt::format("{}%", static_cast<int>(std::lround(cky::hudOpacity * 100.f))));
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
        case B_HUDOP_DN:
        case B_HUDOP_UP:
            // in-level HUD only; the menus stay opaque. ckyHudRefresh() (end of this handler) redraws the HUD.
            cky::hudOpacity = std::clamp(std::round((cky::hudOpacity + (t == B_HUDOP_UP ? 0.05f : -0.05f)) * 100.f) / 100.f, 0.1f, 1.f);
            cky::hudOpacitySave();
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
