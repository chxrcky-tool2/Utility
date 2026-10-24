#include "cky.hpp"
#include <Geode/modify/PlayLayer.hpp>

class $modify(CkyAutoClickHook, PlayLayer) {
    void update(float dt) {
        PlayLayer::update(dt);
        if (cky::autoClicker && !cky::frozen && PlayLayer::get()) {
            float interval = 1.0f / cky::autoClickerCps;
            cky::clickAccumulator += dt;
            while (cky::clickAccumulator >= interval) {
                cky::clickAccumulator -= interval;
                cky::injecting = true;
                GJBaseGameLayer::handleButton(true, 1, true);
                GJBaseGameLayer::handleButton(false, 1, true);
                cky::injecting = false;
            }
        } else {
            cky::clickAccumulator = 0.0f;
        }
    }
};
