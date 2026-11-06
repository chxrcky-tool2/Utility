// main.cpp — ChxrCky Macro Bot — game hooks
#include "state.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <tuple>

// true  = this file applies the speedhack (inside GJBaseGameLayer::update)
// false = set this if physics.cpp already applies speed, so it isn't applied twice
static constexpr bool SPEED_IN_MAIN = true;

namespace {
    float g_diagTimer = 0.f;

    // m_gameState.m_currentProgress is the game's own step counter; only used for the log.
    template <class S>
    int ckyProgress(S const& st) {
        if constexpr (requires { st.m_currentProgress; }) return static_cast<int>(st.m_currentProgress);
        else return -1;
    }

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

    // vertical velocity access that still compiles if a field is missing in this SDK
    template <class P>
    double ckyGetYVel(P* p) {
        if constexpr (requires { p->m_yVelocity; }) return static_cast<double>(p->m_yVelocity);
        else return 0.0;
    }
    template <class P>
    void ckySetYVel(P* p, double v) {
        if constexpr (requires { p->m_yVelocity = v; }) p->m_yVelocity = v;
    }

    // previous-position access that still compiles if the field is missing in this SDK
    template <class P>
    void ckySetLastPos(P* p, CCPoint pt) {
        if constexpr (requires { p->m_lastPosition = pt; }) p->m_lastPosition = pt;
    }

    // Frame fix: snapshot of both players (position, rotation, vertical velocity) taken at the
    // start of a physics step, before that step's inputs are handled.
    void ckyCaptureFix(GJBaseGameLayer* l, uint32_t frame) {
        if (!l->m_player1 || !l->m_player2 || l->m_player1->m_isDead) return;
        if (!cky::fixes.empty() && cky::fixes.back().frame >= frame) return; // one per frame
        FrameFix x{};
        x.frame = frame;
        x.x1 = l->m_player1->getPositionX(); x.y1 = l->m_player1->getPositionY();
        x.r1 = l->m_player1->getRotation();
        x.x2 = l->m_player2->getPositionX(); x.y2 = l->m_player2->getPositionY();
        x.r2 = l->m_player2->getRotation();
        x.yv1 = ckyGetYVel(l->m_player1);
        x.yv2 = ckyGetYVel(l->m_player2);
        cky::fixes.push_back(x);
    }

    // put both players back on the recorded path
    void ckyApplyFix(GJBaseGameLayer* l, FrameFix const& f) {
        if (f.x1 != 0.f || f.y1 != 0.f) {
            l->m_player1->setPosition(CCPoint(f.x1, f.y1));
            ckySetLastPos(l->m_player1, CCPoint(f.x1, f.y1));
            l->m_player1->setRotation(f.r1);
            if (cky::fixVelocity && f.hasVel) ckySetYVel(l->m_player1, f.yv1);
        }
        if (l->m_gameState.m_isDualMode && (f.x2 != 0.f || f.y2 != 0.f)) {
            l->m_player2->setPosition(CCPoint(f.x2, f.y2));
            ckySetLastPos(l->m_player2, CCPoint(f.x2, f.y2));
            l->m_player2->setRotation(f.r2);
            if (cky::fixVelocity && f.hasVel) ckySetYVel(l->m_player2, f.yv2);
        }
    }

    // Exact-frame state check. Called on the step a recorded fix belongs to, BEFORE that step's
    // inputs are injected. Compares the live player state with the recording and only touches the
    // player when it is really off, so a healthy run is never perturbed by re-writing positions.
    // Returns true if a correction was applied.
    bool ckyVerifyFix(GJBaseGameLayer* l, FrameFix const& f) {
        auto check = [&](PlayerObject* p, float fx, float fy, double fyv) {
            double dx = std::abs(static_cast<double>(p->getPositionX()) - fx);
            double dy = std::abs(static_cast<double>(p->getPositionY()) - fy);
            double dv = f.hasVel ? std::abs(ckyGetYVel(p) - fyv) : 0.0;
            return std::tuple<bool, double, double, double>{
                dx > cky::verifyTolPos || dy > cky::verifyTolPos || dv > cky::verifyTolVel, dx, dy, dv };
        };
        bool bad = false;
        double dx = 0, dy = 0, dv = 0;
        if (f.x1 != 0.f || f.y1 != 0.f) {
            auto [b, x, y, v] = check(l->m_player1, f.x1, f.y1, f.yv1);
            bad |= b; dx = x; dy = y; dv = v;
        }
        if (l->m_gameState.m_isDualMode && (f.x2 != 0.f || f.y2 != 0.f)) {
            auto [b, x, y, v] = check(l->m_player2, f.x2, f.y2, f.yv2);
            if (b && !bad) { dx = x; dy = y; dv = v; }
            bad |= b;
        }
        ++cky::verifyChecks;
        if (!bad) return false;

        ++cky::verifyMisses;
        if (cky::firstDivFrame < 0) {
            cky::firstDivFrame = f.frame;
            log::warn("[cky] first desync at frame {}: dx={:.5f} dy={:.5f} dyVel={:.5f}", f.frame, dx, dy, dv);
            ckyOverlayShow(fmt::format("Desync @ frame {} (dy {:.3f}, dvy {:.3f})", f.frame, dy, dv), false);
        }
        return true;
    }

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

        // Fallback: ask the game to draw this frame itself in case it skipped the draw call
        // (compiles away if this SDK has no updateDebugDraw)
        if constexpr (requires { pl->updateDebugDraw(); }) pl->updateDebugDraw();

        // Diagnostic: one line in the Geode log every ~5 s while hitboxes are on
        static uint32_t s_diag = 0;
        if ((s_diag++ % 300) == 0) {
            auto node = pl->m_debugDrawNode;
            log::info("[cky] hitbox diag: enabled={} node={} parent={} visible={} practice={} gameVar0166={}",
                pl->m_isDebugDrawEnabled, node != nullptr,
                node ? (node->getParent() != nullptr) : false,
                node ? node->isVisible() : false,
                pl->m_isPracticeMode,
                GameManager::get() ? GameManager::get()->getGameVariable("0166") : false);
        }
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
        // paused stepper: one press = exactly one step, and no leftover time carried over
        // (leftover time after a back step made the game replay all the way to the old frame)
        if (cky::frozen) steps = std::min(steps, 1);
        double result = steps * modifier;

        m_extraDelta = cky::frozen ? 0.f : static_cast<float>(total - result);
        return static_cast<float>(result);
    }

    void update(float dt) {
        bool inLevel = PlayLayer::get() != nullptr;
        bool singleRun = false;      // this update is one paused frame-step press
        uint32_t stepBefore = cky::step;
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
                if (cky::resimLeft > 0) {
                    // back step: re-run the physics from the loaded anchor up to the target step
                    cky::resimming = true;
                    int n = cky::resimLeft;
                    cky::resimLeft = 0;
                    float stepDt = (cky::effTps() > 240.f ? 1.0005f : 1.f) / cky::effTps();
                    for (int i = 0; i < n; ++i) {
                        if (!m_player1 || m_player1->m_isDead) break;
                        HitboxScope hb;
                        TpsScope tps(this);
                        GJBaseGameLayer::update(stepDt);
                    }
                    cky::resimming = false;
                    ckyBackStepFinish();
                    return;
                }
                if (cky::pending <= 0) return;
                --cky::pending;
                singleRun = true;
                stepBefore = cky::step;
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

            // a single frame-step press should move the counter by exactly 1
            if (singleRun && cky::step > stepBefore + 1) {
                log::warn("[cky] frame step ran {} steps (step {} -> {}, extraDelta {:.5f})",
                    cky::step - stepBefore, stepBefore, cky::step, m_extraDelta);
                ckyOverlayShow(fmt::format("Frame step ran {} steps", cky::step - stepBefore), false);
            }

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
        auto pl = PlayLayer::get();
        static uint32_t s_calls = 0, s_half = 0;

        if (pl) {
            cky::trackCheckpoints(pl->m_checkpointArray ? pl->m_checkpointArray->count() : 0);

            // Accuracy fix: GD 2.2081 can call processCommands more than once for the same
            // physics step (half ticks). Our frame counter, playback injection and frame fixes
            // must only run once per real step, otherwise the counter runs ahead by one for every
            // click and the replay slowly desyncs (dies near the end).
            ++s_calls;
            if (isHalfTick) ++s_half;
            if (s_calls <= 6 || (isHalfTick && s_half <= 6)) {
                log::info("[cky] processCommands call#{} step={} progress={} half={} last={} dt={:.6f}",
                    s_calls, cky::step, ckyProgress(m_gameState), isHalfTick, isLastTick, dt);
            }
            if (!isHalfTick && cky::step > 0 && (cky::step % 2400) == 0) {
                log::info("[cky] sync check: step={} progress={} halfTicksSeen={}",
                    cky::step, ckyProgress(m_gameState), s_half);
            }
        }

        if (pl && !isHalfTick && cky::resimming) {
            // back step re-simulation: replay the logged inputs on their exact steps.
            // No recording, playback, fixes or counters run here, only the physics.
            cky::injecting = true;
            for (int i : cky::resimHeld) {
                GJBaseGameLayer::handleButton(true, i & 0xFF, (i & cky::P2_FLAG) == 0);
            }
            cky::resimHeld.clear();
            for (auto const& e : cky::inputLog) {
                if (e.step != cky::step) continue;
                GJBaseGameLayer::handleButton(e.press, e.btn & 0xFF, (e.btn & cky::P2_FLAG) == 0);
            }
            cky::injecting = false;
        } else if (pl && !isHalfTick) {
            ckyBackStepCapture(); // sparse anchor snapshot (every ~1/8 s) at the start of this step
            uint32_t curFrame = static_cast<uint32_t>(std::max(0.0, cky::frameClock) + 0.5);

            // record a frame fix (player positions) once per step, from the first input on
            if (cky::recording && cky::recArmed && cky::frameFixes && !cky::macro.empty()
                && !cky::injecting && m_player1 && m_player2) {
                // interval is counted in 240-TPS steps, scaled to the macro's own TPS
                int stride = std::max(1, static_cast<int>(
                    std::lround(cky::macroTps / 240.f * std::max(1, cky::fixInterval))));
                if ((curFrame % static_cast<uint32_t>(stride)) == 0) ckyCaptureFix(this, curFrame);
            }

            // playback only runs after a level restart (armed), so timing starts at frame 0
            if (cky::playing && cky::playArmed) {
                double now = cky::frameClock - static_cast<double>(cky::frameOffset);

                // frame fixes first: they were recorded before this step's inputs were handled
                if (cky::frameFixes && m_player1 && m_player2 && !m_player1->m_isDead) {
                    uint32_t nowFrame = static_cast<uint32_t>(std::max(0.0, now) + 0.5);
                    FrameFix const* last = nullptr;
                    while (cky::fixCursor < cky::fixes.size()
                           && static_cast<double>(cky::fixes[cky::fixCursor].frame) <= now) {
                        last = &cky::fixes[cky::fixCursor];
                        ++cky::fixCursor;
                    }
                    // Only a fix that belongs to THIS exact step is used. A stale fix (one that was
                    // skipped past) would drag the player back to an older position.
                    if (last && last->frame == nowFrame) {
                        if (!cky::verifyOn || ckyVerifyFix(this, *last)) ckyApplyFix(this, *last);
                    }
                }

                cky::injecting = true;
                if (cky::holdFixPending) {
                    for (int i = 0; i < 0x200; ++i) {
                        if (cky::holdFix[i]) {
                            GJBaseGameLayer::handleButton(true, i & 0xFF, (i & cky::P2_FLAG) == 0);
                            ckyInputLog(true, i);
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
                    ckyInputLog(in.isPress, in.playerButton);
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

        if (!isHalfTick) {
            cky::step = cky::step + 1;
            cky::frameClock = cky::frameClock + cky::clockInc();
        }
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (PlayLayer::get()) {
            if (cky::resimming && !cky::injecting) return;   // re-simulation only uses logged inputs
            if (cky::playing && !cky::injecting) return;

            if (down) ckyTelemetryPress();
            ckyClickSound(down, isPlayer1);
            if (!cky::injecting) ckyInputLog(down, button | (isPlayer1 ? 0 : cky::P2_FLAG));

            // recording only runs after a level restart (armed)
            if (cky::recording && cky::recArmed && !cky::injecting) {
                MacroInput entry;
                entry.frame = static_cast<uint32_t>(cky::frameClock + 0.5);
                entry.isPress = down;
                entry.playerButton = button | (isPlayer1 ? 0 : cky::P2_FLAG);
                cky::macro.push_back(entry);
                // extra fix on every input frame (the moments that matter most for accuracy)
                if (cky::frameFixes) ckyCaptureFix(this, entry.frame);
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
        cky::fixCursor = 0;
        cky::frozen = false;
        cky::pending = 0;
        cky::dirty = false;
        cky::ckpts.clear();
        cky::cpInit = false;
        ckyBackStepClear();
        cky::recArmed = cky::recording;
        cky::playArmed = cky::playing;
        cky::verifyReset();
        ckyInputLogReset();
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
        ckyBackStepClear();
        ckyInputLogReset();
        if (!resumePlay) cky::verifyReset();

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
            cky::fixCursor = 0;
            cky::ckpts.clear();
            cky::cpInit = false;
            cky::ncReset();
            ckyTelemetryReset();
            if (cky::recording) {
                cky::fixes.clear();
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
        if (ckyTrajHandleDeath(player)) return; // trajectory fake player died: not a real death
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
