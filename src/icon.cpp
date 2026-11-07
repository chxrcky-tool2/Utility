// icon.cpp — draggable round Cky button (0.5 opacity idle, 1.0 while held) (main menu + pause screen)
#include "state.hpp"
#include "icons.hpp"
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

    // instant swap between the two pre-built versions: zero per-frame cost
    void setHeld(bool held) {
        if (m_idle) m_idle->setVisible(!held);
        if (m_held) m_held->setVisible(held);
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
        // CCDrawNode ignores node opacity, so the 0.5 look is its own copy drawn with 0.5 alpha
        m_idle = ckyMakeLogo(RADIUS * 2.f, IDLE_ALPHA);
        m_held = ckyMakeLogo(RADIUS * 2.f, 1.f);
        m_idle->setPosition({0.f, 0.f});
        m_held->setPosition({0.f, 0.f});
        this->addChild(m_idle);
        this->addChild(m_held);
        setHeld(false);
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
