// backstep.cpp — frame stepper "back step": rewind one physics step while paused.
//
// How it works: while the stepper is paused, a snapshot of the whole level state (the game's own
// CheckpointObject: players, camera, triggers, groups...) is taken right before every step you
// advance. Back step loads the previous snapshot and re-syncs the macro bot (step counter, clock,
// recorded inputs / playback cursor).
//
// Three modes of snapshotting:
//  - paused:        one snapshot per step you advance (exact one-frame back steps)
//  - playing, light (default): only a few "anchor" snapshots per second, so pausing and pressing
//                   back works right away (it jumps to the last anchor) without lagging the game
//  - playing, full (cky::backAlways, <= 240 TPS): a snapshot every step, exact back steps at once
#include "state.hpp"
#include <deque>

namespace {
    struct Snap {
        CheckpointObject* cp;
        cky::Cp info; // step / clock / macro size when the snapshot was taken
    };
    std::deque<Snap> g_ring;

    constexpr float  kAnchorHz   = 20.f; // light mode: anchor snapshots per second of game time
    constexpr size_t kAnchorKeep = 8;    // light mode: how many anchors are kept (~0.4s at 20 Hz)

    void dropSnap(Snap& s) {
        if (s.cp) s.cp->release();
        s.cp = nullptr;
    }
}

size_t ckyBackStepCount() { return g_ring.size() > 0 ? g_ring.size() - 1 : 0; }

void ckyBackStepClear() {
    for (auto& s : g_ring) dropSnap(s);
    g_ring.clear();
}

// called once per physics step (processCommands) and every update while paused
void ckyBackStepCapture() {
    auto pl = PlayLayer::get();
    if (!pl || !cky::backStepOn) { ckyBackStepClear(); return; }
    // never snapshot every step while running above 240 TPS: it makes the game fall behind
    bool rolling = cky::backAlways && cky::effTps() <= 240.f;
    // playing without the full buffer: only take a snapshot every few steps (an "anchor")
    bool anchorOnly = !cky::frozen && !rolling;
    if (anchorOnly) {
        uint32_t stride = static_cast<uint32_t>(std::max(1L, std::lround(cky::effTps() / kAnchorHz)));
        if ((cky::step % stride) != 0) return;
    }
    if (cky::trajCreating) return;
    if (!pl->m_player1 || pl->m_player1->m_isDead) return;

    // already have this exact step (paused and idle)
    if (!g_ring.empty() && g_ring.back().info.step == cky::step) return;

    // the step counter went backwards or jumped (restart, checkpoint): snapshots are stale
    if (!g_ring.empty() && g_ring.back().info.step > cky::step) {
        log::info("[cky] backstep: stale snapshots cleared (top {} > step {})",
            g_ring.back().info.step, cky::step);
        ckyBackStepClear();
    }

    size_t cpCount = pl->m_checkpointArray ? pl->m_checkpointArray->count() : 0;
    CheckpointObject* cp = pl->createCheckpoint();
    if (!cp) return;
    cp->retain();

    // safety: if this build's createCheckpoint also registers the object as a practice
    // checkpoint, take it back out so the practice checkpoint list is never touched
    if (pl->m_checkpointArray && pl->m_checkpointArray->count() > cpCount) {
        pl->m_checkpointArray->removeLastObject();
    }

    g_ring.push_back({ cp, { cky::step, cky::frameClock, cky::macro.size(), true } });

    size_t maxSnaps = anchorOnly ? kAnchorKeep
                                 : static_cast<size_t>(std::clamp(cky::backDepth, 30, 600)) + 1;
    while (g_ring.size() > maxSnaps) {
        dropSnap(g_ring.front());
        g_ring.pop_front();
    }
}

// go back one step. Returns false if there is nothing further back.
bool ckyBackStep() {
    auto pl = PlayLayer::get();
    if (!pl || !cky::frozen || g_ring.empty()) return false;

    uint32_t from = cky::step;
    size_t before = g_ring.size();

    // top = snapshot of the current step (taken when paused): discard it
    while (g_ring.size() > 1 && g_ring.back().info.step >= cky::step) {
        dropSnap(g_ring.back());
        g_ring.pop_back();
    }
    if (g_ring.empty() || g_ring.back().info.step >= cky::step) return false;

    Snap& s = g_ring.back(); // stays in the ring: it is now the current step
    pl->loadFromCheckpoint(s.cp);
    pl->m_extraDelta = 0.f; // no leftover time from the step we came from

    log::info("[cky] backstep: from step {} -> {} (ring {} -> {}, popped {})",
        from, s.info.step, before, g_ring.size(), before - g_ring.size());

    // re-sync the macro bot with the restored moment
    if (cky::recording && cky::recArmed) {
        cky::rewindTo(s.info);          // cuts the recorded inputs back to this step
    } else if (cky::playing && cky::playArmed) {
        cky::rewindPlaybackTo(s.info);  // moves the playback cursor back
    } else {
        cky::step = s.info.step;
        cky::frameClock = s.info.clock;
    }

    cky::pending = 0;
    cky::ncBlocked = false;
    ckyOverlayShow(fmt::format("Back to frame {}", cky::step), false);
    return true;
}

// END OF FILE (backstep.cpp)
