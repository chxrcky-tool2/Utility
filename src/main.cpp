// main.cpp — ChxrCky Macro Bot — game hooks
#include "state.hpp"
#include "render/ChxrRender.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <chrono>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

// true  = this file applies the speedhack (inside GJBaseGameLayer::update)
// false = set this if physics.cpp already applies speed, so it isn't applied twice
static constexpr bool SPEED_IN_MAIN = true;

namespace {
    float g_diagTimer = 0.f;

    // practice fixes: full player state saved with every practice-mode checkpoint
    struct CpState { CkyPlayerState p1, p2; };
    std::unordered_map<CheckpointObject*, CpState> g_cpStates;

    // where in the macro each practice checkpoint was placed (step, clock, number of inputs).
    // Keyed by the checkpoint object itself, so a restart resumes from exactly the checkpoint the
    // game loads (the last one in its list) - not from a guess made by counting checkpoints.
    std::unordered_map<CheckpointObject*, cky::Cp> g_cpInfo;

    // game time in steps (m_levelTime * TPS), only used to compare with our own counter in the log
    template <class S>
    double ckyLevelFrame(S const& st) {
        if constexpr (requires { st.m_levelTime; }) return static_cast<double>(st.m_levelTime) * cky::effTps();
        else return -1.0;
    }

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
        x.e1 = cky::captureExtra(l->m_player1);
        x.e2 = cky::captureExtra(l->m_player2);
        x.hasExtra = true;
        cky::fixes.push_back(x);
    }

    // put both players back on the recorded path
    void ckyApplyFix(GJBaseGameLayer* l, FrameFix const& f) {
        if (f.x1 != 0.f || f.y1 != 0.f) {
            l->m_player1->setPosition(CCPoint(f.x1, f.y1));
            ckySetLastPos(l->m_player1, CCPoint(f.x1, f.y1));
            l->m_player1->setRotation(f.r1);
            if (cky::fixVelocity && f.hasVel) ckySetYVel(l->m_player1, f.yv1);
            if (cky::fixVelocity && f.hasExtra) cky::applyExtra(l->m_player1, f.e1);
        }
        if (l->m_gameState.m_isDualMode && (f.x2 != 0.f || f.y2 != 0.f)) {
            l->m_player2->setPosition(CCPoint(f.x2, f.y2));
            ckySetLastPos(l->m_player2, CCPoint(f.x2, f.y2));
            l->m_player2->setRotation(f.r2);
            if (cky::fixVelocity && f.hasVel) ckySetYVel(l->m_player2, f.yv2);
            if (cky::fixVelocity && f.hasExtra) cky::applyExtra(l->m_player2, f.e2);
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
        // state flags (on ground / upside down / dashing / jump buffered) must match too
        uint8_t fm = 0;
        if (f.hasExtra) {
            if (f.x1 != 0.f || f.y1 != 0.f) fm |= cky::extraMismatch(cky::captureExtra(l->m_player1), f.e1);
            if (l->m_gameState.m_isDualMode && (f.x2 != 0.f || f.y2 != 0.f))
                fm |= cky::extraMismatch(cky::captureExtra(l->m_player2), f.e2);
            bad |= (fm != 0);
        }
        ++cky::verifyChecks;
        if (!bad) return false;

        ++cky::verifyMisses;
        if (cky::firstDivFrame < 0) {
            cky::firstDivFrame = f.frame;
            log::warn("[cky] first desync at frame {}: dx={:.5f} dy={:.5f} dyVel={:.5f} stateFlagsDiff={:#04x} (our step {}, game time frame {:.1f})",
                f.frame, dx, dy, dv, fm, cky::step, ckyLevelFrame(l->m_gameState));
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
                ckyBuildLabels(PlayLayer::get());
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
                    // Run until the STEP COUNTER reaches the goal, not "n update() calls": one
                    // update() can run 0 or 2 physics steps (float rounding / m_extraDelta), and
                    // counting calls would leave the re-simulation off by a step.
                    uint32_t goal = cky::step + static_cast<uint32_t>(n);
                    int guard = n * 4 + 8;
                    while (cky::step < goal && guard-- > 0) {
                        if (!m_player1 || m_player1->m_isDead) break;
                        HitboxScope hb;
                        TpsScope tps(this);
                        GJBaseGameLayer::update(stepDt);
                    }
                    if (cky::step != goal) {
                        log::warn("[cky] backstep re-sim ended on step {} (wanted {})", cky::step, goal);
                        ckyOverlayShow(fmt::format("Re-sim off: step {} / {}", cky::step, goal), false);
                    }
                    m_extraDelta = 0.f; // drop any leftover time from the re-simulation
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
                // headless physics: cap this frame's catch-up to what the device can run in the budget
                if (cky::headlessActive() && !chxr::Renderer::get().rendering()) {
                    double cost = std::max(0.002, static_cast<double>(cky::stepCostMs));
                    double maxSteps = std::max(60.0, static_cast<double>(std::clamp(cky::headlessBudgetMs, 4.f, 30.f)) / cost);
                    float maxDt = static_cast<float>(maxSteps / static_cast<double>(cky::effTps()));
                    if (dt > maxDt) dt = maxDt;
                }
                // rendering: fixed timestep, one game update per video frame (TPS/fps physics steps)
                if (chxr::Renderer::get().rendering()) dt = 1.f / static_cast<float>(chxr::Renderer::get().fps());
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
                auto t0 = std::chrono::steady_clock::now();
                uint32_t s0 = cky::step;
                GJBaseGameLayer::update(dt);
                uint32_t ran = cky::step - s0;
                if (cky::headlessActive() && ran >= 8) {
                    float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
                    cky::stepCostMs = cky::stepCostMs * 0.9f + (ms / static_cast<float>(ran)) * 0.1f;
                }
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
                log::info("[cky] sync check: step={} progress={} levelTimeFrame={:.1f} halfTicksSeen={}",
                    cky::step, ckyProgress(m_gameState), ckyLevelFrame(m_gameState), s_half);
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

            // display-only extras are skipped in headless mode above 480 TPS (nobody can see them anyway)
            if (!(cky::headlessActive() && cky::effTps() > 480.f)) {
                ckyFwCapture(this);    // frame window calculator: ring of the last few steps (only when enabled)
                ckyTrailCapture(this); // hitbox trail: this step's hitboxes (after any frame fix)
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

            if (!cky::resimming) { // a back-step re-simulation must stay silent
                if (down) ckyTelemetryPress();
                ckyClickSound(down, isPlayer1);
            }
            if (down && !cky::resimming) ckyFwOnPress(this, isPlayer1); // frame window calculator: queued, runs after this frame
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
    // practice fixes: remember the full player state next to every practice checkpoint
    CheckpointObject* createCheckpoint() {
        auto cp = PlayLayer::createCheckpoint();
        if (!cp || cky::anchoring) return cp;

        // forget checkpoints that no longer exist (removed, or dropped by the game)
        if (g_cpInfo.size() > 512 || g_cpStates.size() > 512) {
            std::unordered_set<CheckpointObject*> live;
            if (m_checkpointArray) {
                for (auto c : CCArrayExt<CheckpointObject*>(m_checkpointArray)) live.insert(c);
            }
            for (auto it = g_cpInfo.begin(); it != g_cpInfo.end();) {
                it = live.contains(it->first) ? std::next(it) : g_cpInfo.erase(it);
            }
            for (auto it = g_cpStates.begin(); it != g_cpStates.end();) {
                it = live.contains(it->first) ? std::next(it) : g_cpStates.erase(it);
            }
        }

        // remember where in the recording / playback this checkpoint was placed
        if ((cky::recording && cky::recArmed) || (cky::playing && cky::playArmed)) {
            g_cpInfo[cp] = { cky::step, cky::frameClock, cky::macro.size(), true };
            log::info("[cky] checkpoint placed: step {} inputs {} (checkpoint list {})",
                cky::step, cky::macro.size(), m_checkpointArray ? m_checkpointArray->count() : 0);
        }

        if (!cky::practiceFixes) return cp;
        g_cpStates[cp] = { cky::capturePlayer(m_player1), cky::capturePlayer(m_player2) };
        return cp;
    }

    // ...and put it back after the game loaded the checkpoint (only while recording / playing)
    void loadFromCheckpoint(CheckpointObject* cp) {
        PlayLayer::loadFromCheckpoint(cp);
        if (!cp || !cky::practiceFixes) return;
        if (!((cky::recording && cky::recArmed) || (cky::playing && cky::playArmed))) return;
        // playback resumes from a checkpoint: let go of jump first (what xdBot does after applyData),
        // so the replayed press is a fresh press and is not swallowed or doubled. The held buttons
        // are re-pressed on the next step by holdFixPending.
        if (cky::playing && cky::playArmed) {
            if (m_player1) m_player1->releaseButton(static_cast<PlayerButton>(1));
            if (m_player2) m_player2->releaseButton(static_cast<PlayerButton>(1));
        }
        auto it = g_cpStates.find(cp);
        if (it == g_cpStates.end()) return;
        cky::restorePlayer(m_player1, it->second.p1);
        if (m_gameState.m_isDualMode) cky::restorePlayer(m_player2, it->second.p2);
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        g_cpStates.clear();
        g_cpInfo.clear();
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
        ckyBuildLabels(this);
        ckyRunReset();
        ckyRunBegin();
        ckyApplyAudio();
        ckyApplyHitboxes();
    }

    void resetLevel() {
        // practice mode: resume from the last checkpoint instead of restarting from zero
        bool resume = false;      // recording resumes at the checkpoint
        bool resumePlay = false;  // playback resumes at the checkpoint
        cky::Cp cp{};
        bool cpOk = false;
        if (m_isPracticeMode && m_checkpointArray && m_checkpointArray->count() > 0
            && ((cky::recording && cky::recArmed) || (cky::playing && cky::playArmed))) {
            // the game resumes from the LAST checkpoint in its list: use that same object
            auto last = static_cast<CheckpointObject*>(m_checkpointArray->lastObject());
            auto it = g_cpInfo.find(last);
            if (it != g_cpInfo.end() && it->second.valid) {
                cp = it->second;
                cpOk = true;
            } else {
                log::warn("[cky] restart: the last checkpoint was not placed while recording / playing, starting from zero");
            }
        }
        if (cpOk && cky::recording && cky::recArmed) {
            resume = true;
        } else if (cpOk && cky::playing && cky::playArmed) {
            resumePlay = true;
        }

        PlayLayer::resetLevel();
        chxr::Renderer::get().onRestart();
        ckyRunBegin(); // labels: a new attempt starts here (level start, startpos or checkpoint)
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
        if (ckyTrajHandleDeath(player, object)) return; // trajectory fake player died: not a real death
        if (cky::noclip && object != m_anticheatSpike) {
            cky::ncBlocked = true;
            return;
        }
        ckyRunDeath(); // labels: remember where this run ended
        PlayLayer::destroyPlayer(player, object);
    }

    void levelComplete() {
        ckyRunComplete();
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
        chxr::Renderer::get().notePlayerEnded(3);
        PlayLayer::levelComplete();
    }
};

// create macros/ and the per-theme macro folders in the mod save folder
$on_mod(Loaded) {
    cky::makeMacroFolders();
}

// END OF FILE (main.cpp)
