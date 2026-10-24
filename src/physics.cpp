#include "state.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>

class $modify(CkyPhysicsHook, GJBaseGameLayer) {
    float getModifiedDelta(float dt) {
        if ((cky::effTps() == 240.f && !cky::frozen) || !PlayLayer::get()) return GJBaseGameLayer::getModifiedDelta(dt);
        float modifier = cky::effTps();
        float total = dt + m_extraDelta;
        int steps = static_cast<int>(total / modifier + 0.0001f);
        float result = steps * modifier;
        m_extraDelta = total - result;
        return result;
    }
};

class $modify(CkySchedulerHook, cocos2d::CCScheduler) {
    void update(float dt) {
        if (cky::speedOn && !cky::frozen && PlayLayer::get()) dt *= cky::speed;
        cocos2d::CCScheduler::update(dt);
    }
};
