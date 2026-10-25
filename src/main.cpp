// main.cpp — ChxrCky Macro Bot — game hooks
#include "state.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

// true  = this file applies the speedhack (inside GJBaseGameLayer::update)
// false = set this if physics.cpp already applies speed, so it isn't applied twice
static constexpr bool SPEED_IN_MAIN = true;

class $modify(CkyGameLayer, GJBaseGameLayer) {
    // TPS bypass (reads the value every frame, so changes apply instantly)
    float getModifiedDelta(float dt) {
        if ((cky::effTps() == 240.f && !cky::frozen) || !PlayLayer::get()) {
            return GJBaseGameLayer::getModifiedDelta(dt);
        }
        float warp = std::min(1.f, m_gameState.m_timeWarp);
        float modifier = warp / cky::effTps();
        float total = dt + m_extraDelta;
        int steps = static_cast<int>(total / modifier + 0.0001f);
        float result = steps * modifier;
        m_extraDelta = total - result;
        return result;
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
                dt = 1.f / cky::effTps();
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
        GJBaseGameLayer::update(dt);
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
                double now = cky::frameClock;
                cky::injecting = true;
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

            // recording only runs after a level restart (armed)
            if (cky::recording && cky::recArmed && !cky::injecting) {
                MacroInput entry;
                entry.frame = static_cast<uint32_t>(cky::frameClock + 0.5);
                entry.isPress = down;
                entry.playerButton = button | (isPlayer1 ? 0 : cky::P2_FLAG);
                cky::macro.push_back(entry);
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
        ckyApplyAudio();
        ckyApplyHitboxes();
    }

    void resetLevel() {
        // practice mode: resume from the last checkpoint instead of wiping the macro
        bool resume = false;
        cky::Cp cp{};
        if (cky::recording && cky::recArmed && m_isPracticeMode && m_checkpointArray
            && m_checkpointArray->count() > 0
            && !cky::ckpts.empty() && cky::ckpts.back().valid) {
            resume = true;
            cp = cky::ckpts.back();
        }

        PlayLayer::resetLevel();

        if (resume) {
            size_t before = cky::macro.size();
            cky::rewindTo(cp);
            if (cky::ckptTelemetry) {
                ckyNotify(fmt::format("restarted from {} to {}",
                    before, cky::macro.size()).c_str());
            }
        } else {
            cky::step = 0;
            cky::frameClock = 0.0;
            cky::cursor = 0;
            cky::ckpts.clear();
            cky::cpInit = false;
            cky::ncReset();
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

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (cky::noclip && object != m_anticheatSpike) {
            cky::ncBlocked = true;
            return;
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void levelComplete() {
        if (cky::safeMode && (cky::dirty || cky::cheating())) {
            ckyNotify("Safe mode: completion blocked", false);
            this->resetLevel();
            return;
        }
        PlayLayer::levelComplete();
    }
};

// END OF FILE (main.cpp)
