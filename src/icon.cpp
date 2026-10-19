// icon.cpp — draggable circular Cky button (main menu + pause screen)
#include "state.hpp"
#include "icons.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

class CkyIcon : public CCLayer {
    bool    m_moved = false;
    CCPoint m_start;
    CCPoint m_grab;
    static constexpr float RADIUS = 22.f;

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
        auto logo = ckyMakeLogo(RADIUS * 2.f);
        logo->setPosition({0.f, 0.f});
        this->addChild(logo);
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
        if (m_moved) {
            cky::savePos("cky-icon", this->getPosition());
        } else {
            ckyOpenPanel(this->getParent());
        }
    }

    void ccTouchCancelled(CCTouch*, CCEvent*) override {
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
