// startpos.cpp — StartPos switcher (bottom-left, behind the checkpoint buttons)
#include "state.hpp"
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <vector>

static constexpr float SP_X = 170.f;
static constexpr float SP_Y = 32.f;

class CkySpSwitcher : public CCNode {
    std::vector<Ref<StartPosObject>> m_pos;
    CCLabelBMFont* m_label = nullptr;
    int m_idx = -1; // -1 = level start

public:
    static CkySpSwitcher* create(std::vector<StartPosObject*> const& v) {
        auto r = new CkySpSwitcher();
        if (r && r->setup(v)) { r->autorelease(); return r; }
        CC_SAFE_DELETE(r);
        return nullptr;
    }

    bool setup(std::vector<StartPosObject*> const& v) {
        if (!CCNode::init()) return false;
        for (auto p : v) m_pos.emplace_back(p);
        this->setID("cky-startpos");
        this->setPosition({SP_X, SP_Y});

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        this->addChild(menu);

        // big arrows for easy touch
        auto mk = [&](char const* t, SEL_MenuHandler cb, float x) {
            auto s = ButtonSprite::create(t);
            s->setScale(0.85f);
            auto b = CCMenuItemSpriteExtra::create(s, this, cb);
            b->setPosition({x, 0.f});
            menu->addChild(b);
        };
        mk("<", menu_selector(CkySpSwitcher::onPrev), -72.f);
        mk(">", menu_selector(CkySpSwitcher::onNext),  72.f);

        m_label = CCLabelBMFont::create("", "bigFont.fnt");
        m_label->setScale(0.5f);
        this->addChild(m_label);
        refresh();
        return true;
    }

    void refresh() {
        if (!m_label) return;
        int n = static_cast<int>(m_pos.size());
        m_label->setString(m_idx < 0
            ? "Start"
            : fmt::format("SP {}/{}", m_idx + 1, n).c_str());
    }

    void goTo(int idx) {
        auto pl = PlayLayer::get();
        if (!pl) return;
        if (cky::recording || cky::playing) {
            ckyNotify("Stop record/playback first", false);
            return;
        }
        m_idx = idx;
        pl->setStartPosObject(idx < 0 ? nullptr : m_pos[idx].data());
        pl->resetLevelFromStart();
        refresh();
    }

    void onNext(CCObject*) {
        int n = static_cast<int>(m_pos.size());
        goTo(((m_idx + 2) % (n + 1)) - 1);
    }

    void onPrev(CCObject*) {
        int n = static_cast<int>(m_pos.size());
        goTo(((m_idx + 1 + n) % (n + 1)) - 1);
    }
};

void ckySetStartPosVisible(bool on) {
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_uiLayer) return;
    if (auto n = pl->m_uiLayer->getChildByID("cky-startpos")) n->setVisible(on);
}

class $modify(CkyStartPosLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        std::vector<StartPosObject*> found;
        if (m_objects) {
            for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
                if (obj && obj->m_objectID == 31) {
                    found.push_back(static_cast<StartPosObject*>(obj));
                }
            }
        }
        if (found.empty() || !m_uiLayer) return;

        std::sort(found.begin(), found.end(), [](StartPosObject* a, StartPosObject* b) {
            return a->getPositionX() < b->getPositionX();
        });

        if (auto old = m_uiLayer->getChildByID("cky-startpos")) old->removeFromParent();
        auto node = CkySpSwitcher::create(found);
        if (!node) return;
        node->setVisible(cky::startPosOn);
        m_uiLayer->addChild(node, 1);
    }
};

// END OF FILE (startpos.cpp)
