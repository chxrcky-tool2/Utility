// labels.cpp — on-screen labels: FPS, startpos run (like 12-56), level time, best run.
// (CPS lives in overlay.cpp and the noclip accuracy / deaths in hud.cpp; all of their on/off
// switches are in the Labels tab of the panel and of the ImGui menu, and are saved.)
#include "state.hpp"

namespace {
    float curPercent() {
        auto pl = PlayLayer::get();
        if (!pl) return 0.f;
        return std::clamp(pl->getCurrentPercent(), 0.f, 100.f);
    }

    // m_levelTime read that still compiles if a game state has no such member
    template <class S>
    double levelSeconds(S const& st) {
        if constexpr (requires { st.m_levelTime; }) return static_cast<double>(st.m_levelTime);
        else return 0.0;
    }

    // run = from where the attempt started (level start, startpos or practice checkpoint) to where it ended
    float g_start = 0.f, g_end = 0.f;
    bool  g_active = false;
    bool  g_hasBest = false;
    float g_bestStart = 0.f, g_bestEnd = 0.f;

    void considerBest() {
        float span = g_end - g_start;
        float bestSpan = g_bestEnd - g_bestStart;
        if (!g_hasBest || span > bestSpan + 0.0001f
            || (std::abs(span - bestSpan) <= 0.0001f && g_end > g_bestEnd)) {
            g_hasBest = true;
            g_bestStart = g_start;
            g_bestEnd = g_end;
        }
    }

    std::string runText(float a, float b) {
        return fmt::format("{}-{}", static_cast<int>(a), static_cast<int>(b));
    }
}

// new level opened: forget the best run
void ckyRunReset() {
    g_active = false;
    g_hasBest = false;
    g_start = g_end = 0.f;
    g_bestStart = g_bestEnd = 0.f;
}

// the player is back at the start of an attempt
void ckyRunBegin() {
    g_start = curPercent();
    g_end = g_start;
    g_active = true;
}

void ckyRunDeath() {
    if (!g_active) return;
    g_end = curPercent();
    g_active = false;
    considerBest();
}

void ckyRunComplete() {
    if (!g_active) return;
    g_end = 100.f;
    g_active = false;
    considerBest();
}

class CkyLabelsNode : public CCNode {
    CCLabelBMFont* m_fps = nullptr;
    CCLabelBMFont* m_run = nullptr;
    CCLabelBMFont* m_time = nullptr;
    CCLabelBMFont* m_best = nullptr;
    float m_acc = 0.f;
    int   m_frames = 0;
    int   m_fpsShown = 0;

    CCLabelBMFont* makeLabel() {
        auto l = CCLabelBMFont::create("", "bigFont.fnt");
        l->setScale(0.35f);
        l->setAnchorPoint({ 0.f, 0.5f });
        l->setOpacity(220);
        this->addChild(l);
        return l;
    }

    // shows or hides one label and stacks the visible ones from top to bottom
    static void place(CCLabelBMFont* l, bool on, std::string const& text, float& y) {
        l->setVisible(on);
        if (!on) return;
        if (text != l->getString()) l->setString(text.c_str());
        l->setPositionY(y);
        y -= 16.f;
    }

public:
    static CkyLabelsNode* create() {
        auto r = new CkyLabelsNode();
        if (r && r->init()) { r->autorelease(); return r; }
        CC_SAFE_DELETE(r);
        return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("cky-labels");
        m_fps = makeLabel();
        m_run = makeLabel();
        m_time = makeLabel();
        m_best = makeLabel();
        this->scheduleUpdate();
        return true;
    }

    void update(float dt) override {
        m_acc += dt;
        ++m_frames;
        if (m_acc >= 0.5f) {
            m_fpsShown = static_cast<int>(std::lround(static_cast<float>(m_frames) / m_acc));
            m_acc = 0.f;
            m_frames = 0;
        }

        float y = 0.f;
        place(m_fps, cky::lblFps, fmt::format("FPS {}", m_fpsShown), y);

        if (cky::lblRun) {
            float end = g_active ? curPercent() : g_end;
            place(m_run, true, "Run " + runText(g_start, end), y);
        } else {
            place(m_run, false, "", y);
        }

        if (cky::lblTime) {
            double t = 0.0;
            if (auto pl = PlayLayer::get()) t = std::max(0.0, levelSeconds(pl->m_gameState));
            int m = static_cast<int>(t / 60.0);
            place(m_time, true, fmt::format("Time {}:{:05.2f}", m, t - m * 60.0), y);
        } else {
            place(m_time, false, "", y);
        }

        place(m_best, cky::lblBest,
            g_hasBest ? "Best " + runText(g_bestStart, g_bestEnd) : std::string("Best -"), y);
    }
};

void ckyBuildLabels(PlayLayer* pl) {
    if (!pl || !pl->m_uiLayer) return;
    pl->m_uiLayer->removeChildByID("cky-labels");
    auto win = CCDirector::get()->getWinSize();
    if (auto n = CkyLabelsNode::create()) {
        n->setPosition({ 14.f, win.height - 100.f });
        pl->m_uiLayer->addChild(n, 90);
    }
}

$on_mod(Loaded) {
    cky::labelsLoad();
}

// END OF FILE (labels.cpp)
