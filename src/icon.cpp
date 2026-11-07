// icon.cpp — draggable round Cky button (0.5 opacity idle, 1.0 while held) (main menu + pause screen)
// Matrix (.glitch): glitching wireframe sphere, flickers 1.0 <-> 0.2 while held.
// Cyber  (.tech):   the round logo with a neon ring spinning around it (fast + 1.0 while held, slow + 0.5 idle).
#include "state.hpp"
#include "icons.hpp"
#include "fx.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

class CkyIcon : public CCLayer {
    bool    m_moved = false;
    CCPoint m_start;
    CCPoint m_grab;
    static constexpr float RADIUS = 22.f;
    static constexpr float IDLE_ALPHA = 0.5f;   // resting opacity; 1.0 the instant it is touched
    CCNode* m_idle = nullptr;
    CCNode* m_held = nullptr;
    // theme extras. CCDrawNode ignores node opacity, so every opacity level is its own pre-built copy, built ONCE
    // (Matrix: 1.0 / 0.5 / 0.2 spheres + a red ghost, Cyber: a 0.5 and a 1.0 ring). Showing one is setVisible().
    CCNode* m_dim = nullptr;
    CCNode* m_ghost = nullptr;
    CCNode* m_ringIdle = nullptr;
    CCNode* m_ringHeld = nullptr;
    cky::fx::Kind m_kind = cky::fx::Kind::None;
    int   m_builtTheme = -1;
    bool  m_isHeld = false;
    float m_angle = 0.f, m_acc = 0.f;

    // wireframe sphere around (0,0): outline, 2 + 1 meridians, 3 latitudes. ~145 segments, built once per copy.
    static CCNode* wireSphere(float alpha, float cr, float cg, float cb) {
        auto d = CCDrawNode::create();
        ccColor4F col = ccc4f(cr * alpha, cg * alpha, cb * alpha, alpha);
        constexpr float TAU = 6.28318530718f;
        float R = RADIUS - 2.f;
        auto ellipse = [&](float cy, float rx, float ry) {
            constexpr int N = 24;
            CCPoint prev(rx, cy);
            for (int i = 1; i <= N; ++i) {
                float a = TAU * static_cast<float>(i) / N;
                CCPoint p(std::cos(a) * rx, cy + std::sin(a) * ry);
                d->drawSegment(prev, p, 0.55f, col);
                prev = p;
            }
        };
        ellipse(0.f, R, R);                                   // outline
        ellipse(0.f, R * 0.45f, R);                           // meridians
        ellipse(0.f, R * 0.80f, R);
        d->drawSegment({ 0.f, -R }, { 0.f, R }, 0.55f, col);
        for (float f : { -0.55f, 0.f, 0.55f }) {              // latitudes (seen from slightly above)
            float rx = R * std::sqrt(1.f - f * f);
            ellipse(R * f, rx, rx * 0.28f);
        }
        return d;
    }

    // three neon arcs with a bright head each, around the logo. Built once; spinning is just setRotation().
    static CCNode* spinRing(float alpha) {
        auto d = CCDrawNode::create();
        constexpr float TAU = 6.28318530718f;
        auto pm = [&](float r, float g, float b, float a) { a *= alpha; return ccc4f(r * a, g * a, b * a, a); };
        float r0 = RADIUS + 1.2f, r1 = r0 + 2.2f, r2 = r1 + 2.4f;
        auto arc = [&](float a0, float a1, float ri, float ro, ccColor4F c) {
            int seg = std::max(3, static_cast<int>((a1 - a0) / 0.12f));
            for (int s = 0; s < seg; ++s) {
                float p = a0 + (a1 - a0) * static_cast<float>(s) / seg, q = a0 + (a1 - a0) * static_cast<float>(s + 1) / seg;
                CCPoint v[4] = { { std::cos(p) * ro, std::sin(p) * ro }, { std::cos(q) * ro, std::sin(q) * ro },
                                 { std::cos(q) * ri, std::sin(q) * ri }, { std::cos(p) * ri, std::sin(p) * ri } };
                d->drawPolygon(v, 4, c, 0.f, c);
            }
        };
        for (int i = 0; i < 3; ++i) {
            float s = TAU * static_cast<float>(i) / 3.f, e = s + 1.45f;
            arc(s, e, r0, r1, pm(0.1f, 0.95f, 1.f, 1.f));     // neon core
            arc(s, e, r1, r2, pm(0.f, 0.8f, 1.f, 0.25f));     // soft halo (does not overlap the core)
            d->drawDot({ std::cos(e) * (r0 + r1) * 0.5f, std::sin(e) * (r0 + r1) * 0.5f }, 1.7f, pm(0.8f, 1.f, 1.f, 1.f));
        }
        return d;
    }

    void showOnly(CCNode* n) {
        for (CCNode* c : { m_idle, m_held, m_dim }) if (c) c->setVisible(c == n);
    }
    void resetGlitch() {
        for (CCNode* c : { m_idle, m_held, m_dim, m_ghost }) if (c) { c->setPosition({ 0.f, 0.f }); c->setScaleX(1.f); }
        if (m_ghost) m_ghost->setVisible(false);
    }

    // (re)builds the look for the current theme. Runs when the button is created and if the theme changes.
    void build() {
        this->unscheduleUpdate();
        this->removeAllChildrenWithCleanup(true);
        m_idle = m_held = m_dim = m_ghost = m_ringIdle = m_ringHeld = nullptr;
        m_builtTheme = cky::theme;
        m_kind = cky::fx::kindOf(cky::theme);
        if (m_kind == cky::fx::Kind::Matrix) {
            auto face = CCDrawNode::create();                  // dark backing so the wireframe reads on any scene
            face->drawDot({ 0.f, 0.f }, RADIUS, ccc4f(0.f, 0.f, 0.f, 0.65f));
            this->addChild(face);
            m_idle  = wireSphere(IDLE_ALPHA, 0.f, 1.f, 0.35f);
            m_held  = wireSphere(1.0f,       0.f, 1.f, 0.35f);
            m_dim   = wireSphere(0.2f,       0.f, 1.f, 0.35f);
            m_ghost = wireSphere(0.8f,       1.f, 0.15f, 0.15f);   // red ghost shown on glitch frames
            for (CCNode* c : { m_idle, m_held, m_dim, m_ghost }) this->addChild(c);
            this->scheduleUpdate();
        } else {
            m_idle = ckyMakeLogo(RADIUS * 2.f, IDLE_ALPHA);    // CCDrawNode ignores node opacity, so the 0.5 look is its own copy
            m_held = ckyMakeLogo(RADIUS * 2.f, 1.f);
            m_idle->setPosition({ 0.f, 0.f });
            m_held->setPosition({ 0.f, 0.f });
            this->addChild(m_idle);
            this->addChild(m_held);
            if (m_kind == cky::fx::Kind::Cyber) {
                m_ringIdle = spinRing(IDLE_ALPHA);
                m_ringHeld = spinRing(1.f);
                this->addChild(m_ringIdle);
                this->addChild(m_ringHeld);
                this->scheduleUpdate();
            }
        }
        setHeld(m_isHeld);
    }

    void checkTheme(float) {                                   // 2 Hz, one int compare
        if (cky::theme != m_builtTheme) build();
    }

    // instant swap between the pre-built versions: zero per-frame cost
    void setHeld(bool held) {
        m_isHeld = held;
        if (m_kind == cky::fx::Kind::Matrix) {
            resetGlitch();
            showOnly(held ? m_held : m_idle);
            return;
        }
        if (m_idle) m_idle->setVisible(!held);
        if (m_held) m_held->setVisible(held);
        if (m_ringIdle) m_ringIdle->setVisible(!held);
        if (m_ringHeld) m_ringHeld->setVisible(held);
    }

    // Cyber: spin the visible ring (a rotation value, nothing redrawn). Matrix: 20 Hz glitch + held flicker.
    void update(float dt) override {
        if (m_kind == cky::fx::Kind::Cyber) {
            m_angle += (m_isHeld ? 260.f : 40.f) * dt;         // held: spins fast, idle: slow
            if (m_angle >= 360.f) m_angle -= 360.f;
            if (CCNode* r = m_isHeld ? m_ringHeld : m_ringIdle) r->setRotation(m_angle);
            return;
        }
        if (m_kind != cky::fx::Kind::Matrix) return;
        m_acc += dt;
        if (m_acc < 0.05f) return;
        m_acc = 0.f;
        resetGlitch();
        CCNode* base = m_idle;
        if (m_isHeld) base = (cky::fx::rnd() & 1u) ? m_held : m_dim;   // rapid flicker 1.0 <-> 0.2
        showOnly(base);
        if (cky::fx::rnd() % 100 < (m_isHeld ? 55u : 14u)) {           // glitch frame: shove, squash, red ghost
            float dx = cky::fx::rf(-4.f, 4.f), dy = cky::fx::rf(-2.5f, 2.5f);
            base->setPosition({ dx, dy });
            base->setScaleX(cky::fx::rf(0.85f, 1.2f));
            m_ghost->setPosition({ -dx * 0.8f, -dy });
            m_ghost->setVisible(true);
        }
    }

public:
    static CkyIcon* create() {
        auto r = new CkyIcon();
        if (r && r->init()) { r->autorelease(); return r; }
        CC_SAFE_DELETE(r);
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        this->setID("cky-icon");
        build();
        this->schedule(schedule_selector(CkyIcon::checkTheme), 0.5f);
        this->setTouchEnabled(true);
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -600, true);
    }

    CCPoint toParent(CCTouch* t) {
        auto p = this->getParent();
        return p ? p->convertToNodeSpace(t->getLocation()) : t->getLocation();
    }

    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        if (!this->isVisible() || !this->getParent()) return false;
        CCPoint p = toParent(t);
        if (ccpDistance(p, this->getPosition()) > RADIUS * this->getScale()) return false;
        m_moved = false;
        m_start = p;
        m_grab = ccpSub(this->getPosition(), p);
        setHeld(true);                         // pressed / held -> 1.0 right away
        return true;
    }

    void ccTouchMoved(CCTouch* t, CCEvent*) override {
        CCPoint p = toParent(t);
        if (!m_moved && ccpDistance(p, m_start) < 6.f) return;
        m_moved = true;
        CCPoint np = ccpAdd(p, m_grab);
        auto win = CCDirector::get()->getWinSize();
        float r = RADIUS * this->getScale();
        np.x = std::clamp(np.x, r, win.width - r);
        np.y = std::clamp(np.y, r, win.height - r);
        this->setPosition(np);
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        setHeld(false);                        // released -> back to 0.5
        if (m_moved) {
            cky::savePos("cky-icon", this->getPosition());
        } else {
            ckyOpenPanel(this->getParent());
        }
    }

    void ccTouchCancelled(CCTouch*, CCEvent*) override {
        setHeld(false);
        if (m_moved) cky::savePos("cky-icon", this->getPosition());
    }
};

void ckyAddIcon(CCNode* host) {
    if (!host || host->getChildByID("cky-icon")) return;
    auto icon = CkyIcon::create();
    if (!icon) return;
    icon->setPosition(cky::savedPos("cky-icon", ckyDefaultIconPos()));
    icon->setScale(cky::iconScale);
    host->addChild(icon, 1000);
}

class $modify(CkyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        ckyAddIcon(this);
        return true;
    }
};

class $modify(CkyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        ckyAddIcon(this);
    }
};

// END OF FILE (icon.cpp)
