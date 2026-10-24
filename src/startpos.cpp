#include "state.hpp"
#include <algorithm>
#include <vector>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

class CkySpSwitcher : public cocos2d::CCNode {
    std::vector<geode::Ref<StartPosObject>> m_pos; cocos2d::CCLabelBMFont* m_label = nullptr; int m_idx = -1; 
public:
    static CkySpSwitcher* create(std::vector<StartPosObject*> const& v) { auto r = new CkySpSwitcher(); if (r && r->setup(v)) { r->autorelease(); return r; } CC_SAFE_DELETE(r); return nullptr; }
    bool setup(std::vector<StartPosObject*> const& v) {
        if (!CCNode::init()) return false;
        for (auto p : v) m_pos.push_back(p);
        this->setID("cky-startpos"); this->setPosition({170.f, 32.f});
        auto menu = cocos2d::CCMenu::create(); menu->setPosition({0.f, 0.f}); this->addChild(menu);
        auto mk = [&](const char* txt, cocos2d::SEL_MenuHandler cb, float x) { auto s = cocos2d::CCLabelBMFont::create(txt, "bigFont.fnt"); s->setScale(0.6f); auto b = CCMenuItemSpriteExtra::create(s, this, cb); b->setPosition({x, 0.f}); menu->addChild(b); };
        mk("<", menu_selector(CkySpSwitcher::onPrev), -72.f); mk(">", menu_selector(CkySpSwitcher::onNext),  72.f);
        m_label = cocos2d::CCLabelBMFont::create("Start", "bigFont.fnt"); m_label->setScale(0.5f); this->addChild(m_label); refresh(); return true;
    }
    void refresh() { if (m_label) m_label->setString(m_idx < 0 ? "Start" : fmt::format("SP {}/{}", m_idx + 1, m_pos.size()).c_str()); }
    void goTo(int idx) { auto pl = PlayLayer::get(); if (!pl || cky::recording || cky::playing) return; m_idx = idx; pl->setStartPosObject(idx < 0 ? nullptr : m_pos[idx].data()); pl->resetLevelFromStart(); refresh(); }
    void onNext(cocos2d::CCObject*) { goTo(((m_idx + 2) % (m_pos.size() + 1)) - 1); }
    void onPrev(cocos2d::CCObject*) { goTo(((m_idx + 1 + m_pos.size()) % (m_pos.size() + 1)) - 1); }
};

namespace cky {
    void ckyBuildStartPos(PlayLayer* pl) {
        if (!pl || !pl->m_uiLayer || !pl->m_objects || pl->m_uiLayer->getChildByID("cky-startpos")) return;
        std::vector<StartPosObject*> found;
        for (auto obj : geode::cocos::CCArrayExt<GameObject*>(pl->m_objects)) if (obj && obj->m_objectID == 31) found.push_back(static_cast<StartPosObject*>(obj));
        if (found.empty()) return;
        std::sort(found.begin(), found.end(), [](StartPosObject* a, StartPosObject* b) { return a->getPositionX() < b->getPositionX(); });
        if (auto node = CkySpSwitcher::create(found)) pl->m_uiLayer->addChild(node, 10);
    }

    void ckySetStartPosVisible(bool on) {
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_uiLayer) return;
        if (auto n = pl->m_uiLayer->getChildByID("cky-startpos")) n->setVisible(on);
    }
}
