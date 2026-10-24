#include "state.hpp"
#include <Geode/modify/PlayLayer.hpp>

// Put the forward declarations INSIDE the cky namespace
namespace cky {
    void ckyBuildPanel(PlayLayer* pl);
    void ckyBuildHud(PlayLayer* pl);
    void ckyBuildStartPos(PlayLayer* pl);

    // Add the missing functions here
    void ckyApplyHudScale() {
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_uiLayer) return;
        if (auto hud = pl->m_uiLayer->getChildByID("cky-hud")) {
            hud->setScale(cky::hudScale);
        }
    }
    void ckyApplyAudio() {}
    void ckyApplyFps() {}
    void ckyApplyHitboxes() {}
}

class $modify(CkyPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        cky::frameClock = 0.0;
        cky::playbackCursor = 0;
        cky::armed = false;
        
        // These will now perfectly match the forward declarations
        cky::ckyBuildPanel(this);
        cky::ckyBuildHud(this);
        cky::ckyBuildStartPos(this);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        cky::frameClock = 0.0;
        cky::playbackCursor = 0;
        cky::clickAccumulator = 0.0f;
        cky::currentClicks = 0;

        if (cky::recording) { cky::currentMacro.clear(); cky::armed = true; }
        if (cky::playing) { cky::armed = true; }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (cky::noclip && object != m_anticheatSpike) return;
        PlayLayer::destroyPlayer(player, object);
    }
};
