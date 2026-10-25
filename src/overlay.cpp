// overlay.cpp — input overlay + click telemetry (CPS / Best / Total)
#include "state.hpp"
#include <chrono>
#include <deque>

namespace {
    std::deque<double> g_press;

    double nowSec() {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }
}

class CkyClickNode : public CCNode {
    CCLabelBMFont* m_l = nullptr;
    std::string m_last;

public:
    static CkyClickNode* create() {
        auto r = new CkyClickNode();
        if (r && r->init()) { r->autorelease(); return r; }
        CC_SAFE_DELETE(r);
        return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-clicks");
        m_l = CCLabelBMFont::create("", "bigFont.fnt");
        m_l->setScale(0.3f);
        m_l->setAnchorPoint({1.f, 0.5f});
        m_l->setOpacity(200);
        this->addChild(m_l);
        this->scheduleUpdate();
        return true;
    }

    void update(float) override {
        double t = nowSec();
        while (!g_press.empty() && t - g_press.front() > 1.0) g_press.pop_front();
        m_l->setVisible(cky::clickHudOn);
        std::string s = fmt::format("CPS {} | Best {} | Total {}",
            g_press.size(), cky::clicksBest, cky::clicksTotal);
        if (s != m_last) {
            m_last = s;
            m_l->setString(s.c_str());
        }
    }
};

void ckyBuildOverlay(PlayLayer* pl) {
    if (!pl || !pl->m_uiLayer) return;
    auto ui = pl->m_uiLayer;
    ui->removeChildByID("cky-overlay");
    ui->removeChildByID("cky-clicks");
    auto win = CCDirector::get()->getWinSize();

    auto ov = CCLabelBMFont::create("", "bigFont.fnt");
    ov->setID("cky-overlay");
    ov->setScale(0.4f);
    ov->setAnchorPoint({0.f, 0.5f});
    ov->setPosition({14.f, win.height - 70.f});
    ov->setOpacity(0);
    ui->addChild(ov, 90);

    if (auto ck = CkyClickNode::create()) {
        ck->setPosition({win.width - 14.f, win.height - 62.f});
        ui->addChild(ck, 90);
    }
    g_press.clear();
    cky::clicksTotal = 0;
    cky::clicksBest = 0;
}

// flashes text on screen, then fades it out
void ckyOverlayShow(std::string const& text, bool good) {
    if (!cky::overlayOn) return;
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_uiLayer) return;
    auto l = typeinfo_cast<CCLabelBMFont*>(pl->m_uiLayer->getChildByID("cky-overlay"));
    if (!l) return;
    l->stopAllActions();
    l->setString(text.c_str());
    l->setColor(good ? ccc3(120, 255, 160) : ccc3(255, 190, 90));
    l->setOpacity(255);
    l->runAction(CCSequence::create(CCDelayTime::create(1.2f), CCFadeOut::create(0.5f), nullptr));
}

void ckyTelemetryPress() {
    double t = nowSec();
    g_press.push_back(t);
    cky::clicksTotal = cky::clicksTotal + 1;
    while (!g_press.empty() && t - g_press.front() > 1.0) g_press.pop_front();
    if (g_press.size() > cky::clicksBest) {
        cky::clicksBest = static_cast<uint32_t>(g_press.size());
    }
}

void ckyTelemetryReset() {
    g_press.clear();
    cky::clicksTotal = 0;
    cky::clicksBest = 0;
}

// END OF FILE (overlay.cpp)
