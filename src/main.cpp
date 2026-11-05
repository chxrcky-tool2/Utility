// main.cpp — ChxrCky Macro Bot — game hooks
#include "state.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

// true  = this file applies the speedhack (inside GJBaseGameLayer::update)
// false = set this if physics.cpp already applies speed, so it isn't applied twice
static constexpr bool SPEED_IN_MAIN = true;

namespace {
    float g_diagTimer = 0.f;

    // High-TPS bypass (above 240).
    // GD works out its own step count from the delta using the formula
    //     step size = min(1, timeWarp) / 240
    // so just returning a different delta from getModifiedDelta gets re-quantised back to
    // 240 steps per second (that is why the real TPS stayed at ~241). Instead we make the
    // game itself think the step size is 1/TPS by shrinking m_timeWarp for the duration of
    // one update call, then put it back.
    struct TpsScope {
        GJBaseGameLayer* layer = nullptr;
        float orig = 1.f, applied = 1.f;
        bool on = false;
        explicit TpsScope(GJBaseGameLayer* l) {
            float t = cky::effTps();
            if (t <= 240.f || !PlayLayer::get()) return;
            layer = l;
            orig = l->m_gameState.m_timeWarp;
            float eff = std::min(1.f, orig);
            if (!(eff > 0.f)) return;
            applied = eff * 240.f / t;
            l->m_gameState.m_timeWarp = applied;
            on = true;
        }
        ~TpsScope() {
            if (!on) return;
            // a timewarp trigger fired during this update: keep the game's new value
            if (layer->m_gameState.m_timeWarp == applied) layer->m_gameState.m_timeWarp = orig;
        }
    };

    struct HitboxScope {
        bool swapped = false;
        HitboxScope() {
            if (!cky::hitboxes) return;
            auto gm = GameManager::get();
            if (!gm) return;
            if (!gm->getGameVariable("0166")) {
                gm->setGameVariable("0166", true);
                swapped = true;
            }
        }
        ~HitboxScope() {
            if (!swapped) return;
            if (auto gm = GameManager::get()) gm->setGameVariable("0166", false);
        }
    };

    void forceHitboxVisible() {
        if (!cky::hitboxes) return;
        auto pl = PlayLayer::get();
        if (!pl) return;
        pl->m_isDebugDrawEnabled = true;
        if (pl->m_debugDrawNode) pl->m_debugDrawNode->setVisible(true);
    }
}

class $modify(CkyGameLayer, GJBaseGameLayer) {
    // TPS below 240 (and frozen stepping at 240). Above 240 is handled by TpsScope in update().
    float getModifiedDelta(float dt) {
        float t = cky::effTps();
        if (t > 240.f || (t == 240.f && !cky::frozen) || !PlayLayer::get()) {
            return GJBaseGameLayer::getModifiedDelta(dt);
        }
        double warp = std::min(1.f, m_gameState.m_timeWarp);
        if (!(warp > 0.0)) warp = 1.0;
        double modifier = warp / static_cast<double>(t);

        // never try to catch up more than 0.25s of game time in one frame
        double total = std::min(static_cast<double>(dt) + static_cast<double>(m_extraDelta), 0.25);
        double stepsD = std::floor(total / modifier + 1e-4);
        int steps = static_cast<int>(std::max(0.0, stepsD));
        double result = steps * modifier;

        m_extraDelta = static_cast<float>(total - result);
        return static_cast<float>(result);
    }

    void update(float dt) {
        bool inLevel = PlayLayer::get() != nullptr;
        if (inLevel) {
            if (cky::cheating()) cky::dirty = true;

            // self-heal: rebuild the HUD if it is missing
            if ((cky::hudTick % 30) == 0 && !ckyHudNode()) {
                ckyBuildHud(PlayLayer::get());
                ckyBuildStartPos(PlayLayer::get());
            }

            if (cky::frozen) {
                if ((++cky::hudTick % 6) == 0) {
                    ckyHudRefresh();
                    ckyApplyAudio();
                }
                if (cky::pending <= 0) return;
                --cky::pending;
                dt = (cky::effTps() > 240.f ? 1.0005f : 1.f) / cky::effTps();
            } else {
                float realDt = dt;
                float sp = SPEED_IN_MAIN ? cky::effSpeed() : 1.f;
                if (sp != 1.f) dt *= sp;
                cky::measTime += realDt * sp;
                if (cky::measTime >= 0.5f) {
                    cky::actualTps = static_cast<float>(cky::measSteps) / cky::measTime;
                    cky::measSteps = 0;
                    cky::measTime = 0.f;
                }
            }
        }
        if (inLevel) {
            HitboxScope hb;
            {
                TpsScope tps(this);
                GJBaseGameLayer::update(dt);
            }
            forceHitboxVisible();

            // if real TPS is far below the target, say so in the Geode log
            g_diagTimer += dt;
            if (g_diagTimer >= 3.f) {
                if (cky::effTps() > 240.f && cky::actualTps > 0.f
                    && cky::actualTps < cky::effTps() * 0.9f) {
                    log::warn("[cky] TPS target {} but real {:.0f} (timeWarp {:.3f})",
                        static_cast<int>(cky::effTps()), cky::actualTps,
                        m_gameState.m_timeWarp);
                }
                g_diagTimer = 0.f;
            }
        } else {
            GJBaseGameLayer::update(dt);
        }
        if (inLevel && (++cky::hudTick % 6) == 0) {
            ckyHudRefresh();
            if ((cky::hudTick % 30) == 0) ckyApplyAudio();
        }
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (auto pl = PlayLayer::get()) {
            cky::trackCheckpoints(pl->m_checkpointArray ? pl->m_checkpointArray->count() : 0);

            // playback only runs after a level restart (armed), so timing starts at frame 0
            if (cky::playing && cky::playArmed) {
                double now = cky::frameClock - static_cast<double>(cky::frameOffset);
                cky::injecting = true;
                if (cky::holdFixPending) {
                    for (int i = 0; i < 0x200; ++i) {
                        if (cky::holdFix[i]) {
                            GJBaseGameLayer::handleButton(true, i & 0xFF, (i & cky::P2_FLAG) == 0);
                        }
                    }
                    cky::holdFixPending = false;
                }
                while (cky::cursor < cky::macro.size()) {
                    MacroInput const& in = cky::macro[cky::cursor];
                    if (static_cast<double>(in.frame) > now) break;
                    ++cky::cursor;
                    bool p1 = (in.playerButton & cky::P2_FLAG) == 0;
                    int btn = in.playerButton & 0xFF;
                    GJBaseGameLayer::handleButton(in.isPress, btn, p1);
                }
                cky::injecting = false;

                if (cky::cursor >= cky::macro.size()) {
                    cky::playing = false;
                    cky::playArmed = false;
                    ckyNotify("Playback finished");
                }
            }

            if (cky::noclip) {
                cky::ncFrames = cky::ncFrames + 1;
                if (cky::ncBlocked) {
                    cky::ncBad = cky::ncBad + 1;
                    if (!cky::ncPrevBad) cky::ncDeaths = cky::ncDeaths + 1;
                }
                cky::ncPrevBad = cky::ncBlocked;
                cky::ncBlocked = false;
            }
            cky::measSteps = cky::measSteps + 1;
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        cky::step = cky::step + 1;
        cky::frameClock = cky::frameClock + cky::clockInc();
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (PlayLayer::get()) {
            if (cky::playing && !cky::injecting) return;

            if (down) ckyTelemetryPress();
            ckyClickSound(down, isPlayer1);

            // recording only runs after a level restart (armed)
            if (cky::recording && cky::recArmed && !cky::injecting) {
                MacroInput entry;
                entry.frame = static_cast<uint32_t>(cky::frameClock + 0.5);
                entry.isPress = down;
                entry.playerButton = button | (isPlayer1 ? 0 : cky::P2_FLAG);
                cky::macro.push_back(entry);
                ckyOverlayShow(fmt::format("Input {} ({})",
                    cky::macro.size(), down ? "press" : "release"));
            }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

class $modify(CkyPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        cky::step = 0;
        cky::frameClock = 0.0;
        cky::cursor = 0;
        cky::frozen = false;
        cky::pending = 0;
        cky::dirty = false;
        cky::ckpts.clear();
        cky::cpInit = false;
        cky::recArmed = cky::recording;
        cky::playArmed = cky::playing;
        cky::ncReset();
        ckyBuildHud(this);
        ckyBuildStartPos(this);
        ckyBuildOverlay(this);
        ckyApplyAudio();
        ckyApplyHitboxes();
    }

    void resetLevel() {
        // practice mode: resume from the last checkpoint instead of restarting from zero
        bool resume = false;      // recording resumes at the checkpoint
        bool resumePlay = false;  // playback resumes at the checkpoint
        cky::Cp cp{};
        bool cpOk = m_isPracticeMode && m_checkpointArray && m_checkpointArray->count() > 0
            && !cky::ckpts.empty() && cky::ckpts.back().valid;
        if (cpOk && cky::recording && cky::recArmed) {
            resume = true;
            cp = cky::ckpts.back();
        } else if (cpOk && cky::playing && cky::playArmed) {
            resumePlay = true;
            cp = cky::ckpts.back();
        }

        PlayLayer::resetLevel();

        if (resume) {
            size_t before = cky::macro.size();
            cky::rewindTo(cp);
            ckyOverlayShow(fmt::format("Going back to {} inputs from {} inputs.",
                cky::macro.size(), before), false);
        } else if (resumePlay) {
            cky::rewindPlaybackTo(cp);
            ckyOverlayShow(fmt::format("Playback back to step {}", cp.step), false);
        } else {
            cky::step = 0;
            cky::frameClock = 0.0;
            cky::cursor = 0;
            cky::ckpts.clear();
            cky::cpInit = false;
            cky::ncReset();
            ckyTelemetryReset();
            if (cky::recording) {
                if (cky::ckptTelemetry && !cky::macro.empty()) {
                    ckyNotify("No valid checkpoint: macro cleared", false);
                }
                cky::macro.clear();
            }
        }

        // a restart arms recording / playback
        if (cky::recording) cky::recArmed = true;
        if (cky::playing && !cky::playArmed) {
            cky::playArmed = true;
            ckyNotify(fmt::format("Playback armed: {} inputs", cky::macro.size()).c_str());
        }

        cky::pending = 0;
        cky::dirty = false;
        ckyApplyAudio();
        ckyApplyHitboxes();
    }

    void postUpdate(float dt) {
        HitboxScope hb;
        PlayLayer::postUpdate(dt);
        forceHitboxVisible();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (cky::noclip && object != m_anticheatSpike) {
            cky::ncBlocked = true;
            return;
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void levelComplete() {
        // auto-stop: recording ends when you reach the end of the level
        if (cky::recording) {
            cky::recording = false;
            cky::recArmed = false;
            ckyNotify(fmt::format("Recording stopped: {} inputs", cky::macro.size()).c_str());
        }
        if (cky::safeMode && (cky::dirty || cky::cheating())) {
            ckyNotify("Safe mode: completion blocked", false);
            this->resetLevel();
            return;
        }
        PlayLayer::levelComplete();
    }
};

// END OF FILE (main.cpp)
