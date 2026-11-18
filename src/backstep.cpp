#include "state.hpp"
#include <deque>

namespace {
    struct Snap {
        CheckpointObject* cp;
        uint32_t step;
        double clock;
        std::vector<int> held;
        CkyPlayerState p1, p2;
    };
    std::deque<Snap> g_ring;
    uint32_t g_target = 0;
    std::vector<int> g_anchorHeld;

    constexpr float  kAnchorHz = 8.f;
    constexpr size_t kLogCap   = 200000;

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

void ckyBackStepCapture() {
    auto pl = PlayLayer::get();
    if (!pl || !cky::backStepOn) { ckyBackStepClear(); return; }
    if (cky::resimming || cky::trajCreating) return;
    if (!pl->m_player1 || pl->m_player1->m_isDead) return;

    if (!g_ring.empty() && g_ring.back().step > cky::step) {
        log::info("[cky] backstep: stale anchors cleared (top {} > step {})",
            g_ring.back().step, cky::step);
        ckyBackStepClear();
    }

    if (!g_ring.empty() && (cky::step % anchorStride()) != 0) return;
    if (!g_ring.empty() && g_ring.back().step == cky::step) return;

    size_t cpCount = pl->m_checkpointArray ? pl->m_checkpointArray->count() : 0;
    cky::anchoring = true;
    CheckpointObject* cp = pl->createCheckpoint();
    cky::anchoring = false;
    if (!cp) return;
    cp->retain();

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

bool ckyBackStep() {
    auto pl = PlayLayer::get();
    if (!pl || !cky::frozen || g_ring.empty()) return false;

    uint32_t cur = cky::resimLeft > 0 ? g_target : cky::step;
    if (cur == 0) return false;
    uint32_t target = cur - 1;

    int idx = -1;
    for (int i = static_cast<int>(g_ring.size()) - 1; i >= 0; --i) {
        if (g_ring[i].step <= target) { idx = i; break; }
    }
    if (idx < 0) return false;

    while (static_cast<int>(g_ring.size()) - 1 > idx) {
        dropSnap(g_ring.back());
        g_ring.pop_back();
    }

    Snap& s = g_ring.back();
    pl->loadFromCheckpoint(s.cp);
    if (s.p1.valid) cky::restorePlayer(pl->m_player1, s.p1);
    if (s.p2.valid && pl->m_gameState.m_isDualMode) cky::restorePlayer(pl->m_player2, s.p2);
    pl->m_extraDelta = 0.f;

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

void ckyBackStepFinish() {
    cky::resimLeft = 0;
    cky::resimHeld.clear();

    ckyTrailRewind(cky::step);
    if (auto sfLayer = PlayLayer::get()) ckySfSettle(sfLayer, cky::step);

    while (!cky::inputLog.empty() && cky::inputLog.back().step >= cky::step) cky::inputLog.pop_back();

    for (auto& h : cky::held) h = false;
    for (int b : g_anchorHeld) cky::held[b & 0x1FF] = true;
    for (auto const& e : cky::inputLog) cky::held[e.btn & 0x1FF] = e.press;

    uint32_t cf = static_cast<uint32_t>(std::max(0.0, cky::frameClock) + 0.5);
    size_t macroSize = 0;
    while (macroSize < cky::macro.size() && cky::macro[macroSize].frame < cf) ++macroSize;
    cky::Cp info{ cky::step, cky::frameClock, macroSize, true };

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
        cky::rewindTo(info);
    } else if (cky::playing && cky::playArmed) {
        cky::rewindPlaybackTo(info);
        cky::holdFixPending = false;
    }

    cky::pending = 0;
    cky::ncBlocked = false;
    ckyHudRefresh();
    ckyOverlayShow(fmt::format("Back to frame {}", cky::step), false);
}

