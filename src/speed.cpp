// speed.cpp — speedhack: scales game time while you're inside a level
#include "state.hpp"
#include <Geode/modify/CCScheduler.hpp>

static bool ckyPaused() {
    auto scene = CCDirector::get()->getRunningScene();
    if (!scene) return false;
    if (auto kids = scene->getChildren()) {
        for (auto c : CCArrayExt<CCNode*>(kids)) {
            if (typeinfo_cast<PauseLayer*>(c)) return true;
        }
    }
    return false;
}

class $modify(CkyScheduler, CCScheduler) {
    void update(float dt) {
        float sp = cky::effSpeed();
        if (sp != 1.f && !cky::frozen && PlayLayer::get() && !ckyPaused()) {
            dt *= sp;
        }
        CCScheduler::update(dt);
    }
};

// END OF FILE (speed.cpp)
