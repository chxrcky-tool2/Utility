// backstep.cpp — frame stepper "back step": rewind one physics step while paused.
//
// How it works (v2: sparse anchors + deterministic re-simulation):
//  - Every ~1/8 s of game time (30 steps at 240 TPS) ONE full snapshot ("anchor", the game's own
//    CheckpointObject) is taken, in every mode. That is ~30x fewer createCheckpoint() calls than
//    the old "snapshot every step" buffer, which is what made the rolling buffer laggy.
//  - Every input that reaches the game is logged with the physics step it happened on.
//  - Back step to step T: load the newest anchor <= T, then re-run the physics (and re-inject the
//    logged inputs on their exact steps) until step T. That is at most one stride of steps
//    (<= ~30), a tiny cost next to a snapshot, and it is exact because GD physics is deterministic.
//  - The macro bot (recorded inputs / playback cursor / frame fixes) is re-synced at the end.
#include "state.hpp"
#include <deque>

namespace {
    struct Snap {
        CheckpointObject* cp;
        uint32_t step;
        double clock;
        std::vector<int> held; // buttons held when the snapshot was taken (before that step's inputs)
        CkyPlayerState p1, p2; // full player state (the checkpoint alone does not restore all of it)
    };
    std::deque<Snap> g_ring;
    uint32_t g_target = 0;            // step a pending back step is heading to
    std::vector<int> g_anchorHeld;    // held buttons at the anchor in use

    constexpr float  kAnchorHz = 8.f;    // anchors per second of game time
    constexpr size_t kLogCap   = 200000; // hard cap on logged inputs

    uint32_t anchorStride() {
        return static_cast<uint32_t>(std::max(1L, std::lround(cky::effTps() / kAnchorHz)));
    }

    void dropSnap(Snap& s) {
        if (s.cp) s.cp->release();
        s.cp = nullptr;
    }

    void pruneLog() {
        uint32_t floorStep = g_ring.empty() ? cky::step : g_ring.front().step;
        while (!cky::inputLog.empty() && cky::inputLog.front().step < floorStep) cky::inputLog.pop_front();
        while (cky::inputLog.size() > kLogCap) cky::inputLog.pop_front();
    }
}

size_t ckyBackStepCount() {
    if (g_ring.empty()) return 0;
    uint32_t cur = cky::resimLeft > 0 ? g_target : cky::step;
    return cur > g_ring.front().step ? cur - g_ring.front().step : 0;
}

void ckyBackStepClear() {
    for (auto& s : g_ring) dropSnap(s);
    g_ring.clear();
    cky::resimming = false;
    cky::resimLeft = 0;
    cky::resimHeld.clear();
    cky::inputLog.clear();
}

void ckyInputLogReset() {
    cky::inputLog.clear();
    for (auto& h : cky::held) h = false;
}

void ckyInputLog(bool down, int playerButton) {
    int idx = playerButton & 0x1FF;
    cky::held[idx] = down;
    cky::inputLog.push_back({ cky::step, down, idx });
}

// called once per physics step (processCommands), before that step's inputs
void ckyBackStepCapture() {
    auto pl = PlayLayer::get();
    if (!pl || !cky::backStepOn) { ckyBackStepClear(); return; }
    if (cky::resimming || cky::trajCreating) return;
    if (!pl->m_player1 || pl->m_player1->m_isDead) return;

    // the step counter went backwards or jumped (restart, checkpoint): snapshots are stale
    if (!g_ring.empty() && g_ring.back().step > cky::step) {
        log::info("[cky] backstep: stale anchors cleared (top {} > step {})",
            g_ring.back().step, cky::step);
        ckyBackStepClear();
    }

    // anchors only on stride boundaries (the first one is taken immediately)
    if (!g_ring.empty() && (cky::step % anchorStride()) != 0) return;
    if (!g_ring.empty() && g_ring.back().step == cky::step) return;

    size_t cpCount = pl->m_checkpointArray ? pl->m_checkpointArray->count() : 0;
    cky::anchoring = true;
    CheckpointObject* cp = pl->createCheckpoint();
    cky::anchoring = false;
    if (!cp) return;
    cp->retain();

    // safety: if this build's createCheckpoint also registers the object as a practice
    // checkpoint, take it back out so the practice checkpoint list is never touched
    if (pl->m_checkpointArray && pl->m_checkpointArray->count() > cpCount) {
        pl->m_checkpointArray->removeLastObject();
    }

    Snap s{ cp, cky::step, cky::frameClock, {} };
    if (cky::practiceFixes) {
        s.p1 = cky::capturePlayer(pl->m_player1);
        s.p2 = cky::capturePlayer(pl->m_player2);
    }
    for (int i = 0; i < 0x200; ++i) if (cky::held[i]) s.held.push_back(i);
    g_ring.push_back(std::move(s));

    size_t maxSnaps = static_cast<size_t>(std::clamp(cky::backDepth, 30, 600)) / anchorStride() + 2;
    while (g_ring.size() > maxSnaps) {
        dropSnap(g_ring.front());
        g_ring.pop_front();
    }
    pruneLog();
}

// go back one step. Returns false if there is nothing further back.
// The state is restored over the next update(): it loads the anchor now and re-simulates after.
bool ckyBackStep() {
    auto pl = PlayLayer::get();
    if (!pl || !cky::frozen || g_ring.empty()) return false;

    uint32_t cur = cky::resimLeft > 0 ? g_target : cky::step;
    if (cur == 0) return false;
    uint32_t target = cur - 1;

    // newest anchor at or before the target step
    int idx = -1;
    for (int i = static_cast<int>(g_ring.size()) - 1; i >= 0; --i) {
        if (g_ring[i].step <= target) { idx = i; break; }
    }
    if (idx < 0) return false;

    // anchors after the target belong to a future we are leaving
    while (static_cast<int>(g_ring.size()) - 1 > idx) {
        dropSnap(g_ring.back());
        g_ring.pop_back();
    }

    Snap& s = g_ring.back();
    pl->loadFromCheckpoint(s.cp);
    // re-apply everything the checkpoint does not restore (xdBot-style practice fixes)
    if (s.p1.valid) cky::restorePlayer(pl->m_player1, s.p1);
    if (s.p2.valid && pl->m_gameState.m_isDualMode) cky::restorePlayer(pl->m_player2, s.p2);
    pl->m_extraDelta = 0.f; // no leftover time from the step we came from

    cky::step = s.step;
    cky::frameClock = s.clock;
    cky::resimHeld = s.held;
    g_anchorHeld = s.held;
    g_target = target;
    cky::resimLeft = static_cast<int>(target - s.step);
    cky::pending = 0;
    cky::ncBlocked = false;

    log::info("[cky] backstep: {} -> {} via anchor {} ({} steps to re-simulate, ring {})",
        cur, target, s.step, cky::resimLeft, g_ring.size());

    if (cky::resimLeft == 0) ckyBackStepFinish();
    return true;
}

// re-sync the macro bot with the restored moment (runs after the re-simulation)
void ckyBackStepFinish() {
    cky::resimLeft = 0;
    cky::resimHeld.clear();

    ckyTrailRewind(cky::step); // hitbox trail: forget the steps we just went back over

    // inputs from the step we are now at (and later) are in the "future": forget them
    while (!cky::inputLog.empty() && cky::inputLog.back().step >= cky::step) cky::inputLog.pop_back();

    // rebuild the held-button state for this exact step: anchor state + logged inputs before it
    for (auto& h : cky::held) h = false;
    for (int b : g_anchorHeld) cky::held[b & 0x1FF] = true;
    for (auto const& e : cky::inputLog) cky::held[e.btn & 0x1FF] = e.press;

    uint32_t cf = static_cast<uint32_t>(std::max(0.0, cky::frameClock) + 0.5);
    size_t macroSize = 0;
    while (macroSize < cky::macro.size() && cky::macro[macroSize].frame < cf) ++macroSize;
    cky::Cp info{ cky::step, cky::frameClock, macroSize, true };

    // Drift check: after anchor load + re-simulation, how far is the player from the recorded
    // frame fix for this exact step? (must run BEFORE rewindTo, which drops those fixes)
    if (auto pl = PlayLayer::get(); pl && pl->m_player1 && !cky::fixes.empty()) {
        int64_t want = static_cast<int64_t>(cf)
            - ((cky::playing && cky::playArmed) ? static_cast<int64_t>(cky::frameOffset) : 0);
        auto it = std::lower_bound(cky::fixes.begin(), cky::fixes.end(), want,
            [](FrameFix const& f, int64_t v) { return static_cast<int64_t>(f.frame) < v; });
        if (it != cky::fixes.end() && static_cast<int64_t>(it->frame) == want) {
            auto* p = pl->m_player1;
            double dx = std::abs(static_cast<double>(p->getPositionX()) - it->x1);
            double dy = std::abs(static_cast<double>(p->getPositionY()) - it->y1);
            double dv = 0.0;
            if constexpr (requires { p->m_yVelocity; }) {
                if (it->hasVel) dv = std::abs(static_cast<double>(p->m_yVelocity) - it->yv1);
            }
            bool bad = dx > cky::verifyTolPos || dy > cky::verifyTolPos || dv > cky::verifyTolVel;
            log::info("[cky] backstep drift @ frame {}: dx={:.5f} dy={:.5f} dyVel={:.5f} {}",
                want, dx, dy, dv, bad ? "DRIFT" : "ok");
            if (bad) ckyOverlayShow(fmt::format("Backstep drift (dy {:.4f}, dvy {:.4f})", dy, dv), false);
        }
    }

    if (cky::recording && cky::recArmed) {
        cky::rewindTo(info);              // cuts the recorded inputs back to this step
    } else if (cky::playing && cky::playArmed) {
        cky::rewindPlaybackTo(info);      // moves the playback cursor back
        // the re-simulation already re-pressed everything that is held, don't press it twice
        cky::holdFixPending = false;
    }

    cky::pending = 0;
    cky::ncBlocked = false;
    ckyHudRefresh();
    ckyOverlayShow(fmt::format("Back to frame {}", cky::step), false);
}

// END OF FILE (backstep.cpp)
