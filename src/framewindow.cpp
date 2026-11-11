#include "state.hpp"
#include <Geode/modify/PlayLayer.hpp>
#include <array>
#include <chrono>

namespace {
    using Clock = std::chrono::steady_clock;

    constexpr int    MAX_AHEAD = 10;
    constexpr int    MAX_BACK = 10;
    constexpr int    RING = MAX_BACK + 1;
    constexpr double BUDGET_MS = 6.0;
    constexpr int    CONFIRM_STEPS = 30;

    CkySimSnap g_ring[2][RING];
    int        g_head[2] = { -1, -1 };
    CkySimSnap g_back[2][RING];
    int        g_backAvail[2] = { -1, -1 };
    CkySimSnap g_base[2][MAX_AHEAD + 1];
    int        g_baseOk[2] = { 0, 0 };
    bool       g_overflow = false;

    struct Request { bool on = false; int mask = 0; uint32_t step = 0; };
    Request g_req;

    struct Jingle { bool on = false; int mask = 0; uint32_t start = 0, due = 0; };
    Jingle g_jingle;

    CkyFwResult g_res;
    uint32_t    g_resId = 0;

    Ref<PlayerObject> g_a, g_b;

    struct TrajGuard {
        bool prev;
        TrajGuard() { prev = cky::trajCreating; cky::trajCreating = true; }
        ~TrajGuard() { cky::trajCreating = prev; }
    };

    double msSince(Clock::time_point t0) {
        return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    }

    PlayerObject* playerOf(GJBaseGameLayer* l, int pi) { return pi == 0 ? l->m_player1 : l->m_player2; }

    void captureRing(GJBaseGameLayer* l) {
        bool dual = l->m_gameState.m_isDualMode;
        for (int pi = 0; pi < 2; ++pi) {
            if (pi == 1 && !dual) { g_head[1] = -1; continue; }
            PlayerObject* p = playerOf(l, pi);
            if (!p) continue;
            if (g_head[pi] >= 0 && g_ring[pi][g_head[pi]].valid && g_ring[pi][g_head[pi]].step == cky::step) continue;
            int idx = (g_head[pi] + 1) % RING;
            if (!cky::captureSim(g_ring[pi][idx], p)) {
                if (!g_overflow) {
                    g_overflow = true;
                    log::error("[cky] frame window: player snapshot is bigger than CKY_SIM_BYTES ({}); calculator disabled", CKY_SIM_BYTES);
                    ckyNotify("Frame window: snapshot too large (see log)", false);
                }
                return;
            }
            g_ring[pi][idx].step = cky::step;
            g_head[pi] = idx;
        }
    }

    bool stepFake(PlayLayer* pl, PlayerObject* f, float dt, bool pushNow) {
        if (f->m_collisionLogTop)    f->m_collisionLogTop->removeAllObjects();
        if (f->m_collisionLogBottom) f->m_collisionLogBottom->removeAllObjects();
        if (f->m_collisionLogLeft)   f->m_collisionLogLeft->removeAllObjects();
        if (f->m_collisionLogRight)  f->m_collisionLogRight->removeAllObjects();

        pl->checkCollisions(f, dt, false);
        if (ckyTrajCancelled()) return false;

        if (pushNow) f->pushButton(static_cast<PlayerButton>(1));
        f->update(dt);
        f->updateRotation(dt);
        f->updatePlayerScale();
        return true;
    }

    struct Verdict {
        bool ok = false;
        bool hasKiller = false;
        CCPoint kill{ 0.f, 0.f };
    };

    Verdict runBranch(PlayLayer* pl, CkySimSnap const& s, int steps, float dt) {
        Verdict v;
        PlayerObject* f = g_b.data();
        cky::restoreSim(f, s);
        f->m_isDead = false;
        ckyTrajResetCancel();
        for (int i = 0; i < steps; ++i) {
            if (!stepFake(pl, f, dt, i == 0)) {
                v.hasKiller = ckyTrajKiller(&v.kill);
                return v;
            }
        }
        v.ok = true;
        return v;
    }

    struct Ctx {
        PlayLayer* pl = nullptr;
        float dt = 1.f / 240.f;
        int look = 90;
        int mask = 1;
        int fAvail = 0, bAvail = 0;
        int8_t cache[MAX_BACK + MAX_AHEAD + 1];
        bool hasK[MAX_BACK + MAX_AHEAD + 1];
        CCPoint kpos[MAX_BACK + MAX_AHEAD + 1];
        int sims = 0;
        bool timedOut = false;
        Clock::time_point t0;
    };

    CkySimSnap const* sourceFor(int pi, int o) {
        if (o == 0) return &g_back[pi][0];
        if (o < 0)  return (-o <= g_backAvail[pi]) ? &g_back[pi][-o] : nullptr;
        return (o <= g_baseOk[pi]) ? &g_base[pi][o] : nullptr;
    }

    bool validAt(Ctx& c, int o) {
        int idx = o + MAX_BACK;
        if (c.cache[idx] >= 0) return c.cache[idx] == 1;
        if (msSince(c.t0) > BUDGET_MS) { c.timedOut = true; return false; }
        bool ok = true;
        for (int pi = 0; pi < 2; ++pi) {
            if (!(c.mask & (1 << pi))) continue;
            CkySimSnap const* s = sourceFor(pi, o);
            if (!s) { ok = false; break; }
            Verdict v = runBranch(c.pl, *s, c.look, c.dt);
            ++c.sims;
            if (!v.ok) {
                ok = false;
                c.hasK[idx] = v.hasKiller;
                c.kpos[idx] = v.kill;
                break;
            }
        }
        c.cache[idx] = ok ? 1 : 0;
        return ok;
    }

    void expand(Ctx& c, int center, int& lo, int& hi) {
        int good = center, limit = c.fAvail;
        if (good < limit) {
            if (validAt(c, limit)) good = limit;
            else {
                int bad = limit;
                while (bad - good > 1) {
                    int mid = (good + bad) / 2;
                    if (validAt(c, mid)) good = mid; else bad = mid;
                }
            }
        }
        hi = good;

        good = center;
        limit = -c.bAvail;
        if (good > limit) {
            if (validAt(c, limit)) good = limit;
            else {
                int bad = limit;
                while (good - bad > 1) {
                    int mid = (good + bad) / 2;
                    if (validAt(c, mid)) good = mid; else bad = mid;
                }
            }
        }
        lo = good;
    }

    void playJingle(bool force) {
        static std::string path;
        static int state = 0;
        static double last = -100.0;
        if (state == 0) {
            auto p = cky::stdPath(Mod::get()->getResourcesDir()) / "jingle.wav";
            std::error_code ec;
            if (std::filesystem::exists(p, ec)) { path = p.string(); state = 1; }
            else {
                state = 2;
                log::warn("[cky] frame window: jingle.wav not found at {}", p.string());
            }
        }
        if (state != 1) return;
        double now = cky::fwNow();
        if (!force && now - last < 0.6) return;
        last = now;
        auto engine = FMODAudioEngine::sharedEngine();
        if (!engine) return;
        engine->playEffect(path, 1.f, 1.f, static_cast<float>(cky::fwVol) / 100.f);
    }

    void jingleTick(PlayLayer* pl) {
        if (!g_jingle.on) return;
        if (cky::step < g_jingle.start) { g_jingle.on = false; return; }
        for (int pi = 0; pi < 2; ++pi) {
            if (!(g_jingle.mask & (1 << pi))) continue;
            PlayerObject* p = playerOf(pl, pi);
            if (!p || p->m_isDead) { g_jingle.on = false; return; }
        }
        if (cky::step >= g_jingle.due) {
            g_jingle.on = false;
            playJingle(false);
        }
    }

    bool ensureFakes(PlayLayer* pl) {
        if (g_a && g_b) return true;
        g_a = PlayerObject::create(1, 1, pl, pl, true);
        g_b = PlayerObject::create(1, 1, pl, pl, true);
        for (auto f : { g_a.data(), g_b.data() }) {
            if (!f) continue;
            f->setPosition({ 0.f, 105.f });
            f->setVisible(false);
            pl->m_objectLayer->addChild(f);
        }
        return g_a && g_b;
    }

    void resetAll() {
        g_head[0] = g_head[1] = -1;
        g_backAvail[0] = g_backAvail[1] = -1;
        g_req = Request{};
        g_jingle = Jingle{};
        g_res = CkyFwResult{};
    }

    void runRequest(PlayLayer* pl) {
        if (!g_req.on) return;
        g_req.on = false;
        if (!ensureFakes(pl)) return;

        Clock::time_point t0 = Clock::now();
        TrajGuard guard;

        Ctx c;
        c.pl = pl;
        c.dt = ckyTrajStepDelta();
        if (!(c.dt > 0.f)) c.dt = 1.f / 240.f;
        c.look = std::clamp(cky::fwLook, 20, 240);
        c.mask = g_req.mask;
        c.t0 = t0;
        std::fill(std::begin(c.cache), std::end(c.cache), static_cast<int8_t>(-1));
        std::fill(std::begin(c.hasK), std::end(c.hasK), false);

        int fWanted = std::clamp(cky::fwAhead, 1, MAX_AHEAD);
        int bWanted = std::clamp(cky::fwBack, 0, MAX_BACK);
        c.fAvail = fWanted;
        c.bAvail = bWanted;

        for (int pi = 0; pi < 2; ++pi) {
            if (!(c.mask & (1 << pi))) continue;
            g_baseOk[pi] = 0;
            PlayerObject* a = g_a.data();
            cky::restoreSim(a, g_back[pi][0]);
            a->m_isDead = false;
            ckyTrajResetCancel();
            for (int o = 1; o <= fWanted; ++o) {
                if (!stepFake(pl, a, c.dt, false)) break;
                if (!cky::captureSim(g_base[pi][o], a)) break;
                g_baseOk[pi] = o;
            }
            c.fAvail = std::min(c.fAvail, g_baseOk[pi]);
            c.bAvail = std::min(c.bAvail, std::max(0, g_backAvail[pi]));
        }

        CkyFwResult r;
        r.stepHz = cky::effTps();
        int lo = 0, hi = 0;
        bool hit = validAt(c, 0);
        int center = 0;
        if (hit) {
            expand(c, 0, lo, hi);
        } else {
            bool found = false;
            int span = std::max(c.fAvail, c.bAvail);
            for (int d = 1; d <= span && !found; ++d) {
                if (d <= c.fAvail && validAt(c, d))  { center = d;  found = true; }
                else if (d <= c.bAvail && validAt(c, -d)) { center = -d; found = true; }
            }
            if (found) {
                expand(c, center, lo, hi);
                r.hasNext = true;
                r.next = center;
            }
        }

        bool anyWindow = hit || r.hasNext;
        if (anyWindow) {
            r.lo = lo;
            r.hi = hi;
            r.width = hi - lo + 1;
            bool hiEdge = (hi == c.fAvail) && (c.fAvail == fWanted);
            bool loEdge = (lo == -c.bAvail);
            r.truncated = hiEdge || loEdge;
        }
        r.hit = hit;
        r.window = anyWindow;
        r.perfect = hit && !r.truncated && r.width <= std::clamp(cky::fwPerfect, 1, 6);
        r.incomplete = c.timedOut;
        r.sims = c.sims;

        bool haveAnchor = false;
        auto tryAnchor = [&](int o) {
            if (haveAnchor || o < -MAX_BACK || o > MAX_AHEAD) return;
            int idx = o + MAX_BACK;
            if (c.cache[idx] == 0 && c.hasK[idx]) { r.anchor = c.kpos[idx]; haveAnchor = true; }
        };
        if (anyWindow) { tryAnchor(hi + 1); tryAnchor(lo - 1); }
        tryAnchor(0);
        if (!haveAnchor) {
            int pi = (c.mask & 1) ? 0 : 1;
            r.anchor = ccp(g_back[pi][0].pos.x, g_back[pi][0].pos.y + 20.f);
        }

        r.valid = true;
        r.ms = msSince(t0);
        r.stamp = cky::fwNow();
        g_res = r;
        ++g_resId;

        if (r.perfect && cky::fwJingle) {
            g_jingle.on = true;
            g_jingle.mask = c.mask;
            g_jingle.start = g_req.step;
            g_jingle.due = cky::step + CONFIRM_STEPS;
        }
    }
}

void ckyFwCapture(GJBaseGameLayer* l) {
    if (!cky::fwOn || g_overflow || !l || cky::trajCreating) return;
    captureRing(l);
}

void ckyFwOnPress(GJBaseGameLayer* l, bool isPlayer1) {
    if (!cky::fwOn || g_overflow || cky::trajCreating || !l) return;
    auto pl = PlayLayer::get();
    if (!pl || static_cast<GJBaseGameLayer*>(pl) != l) return;
    if (pl->m_levelSettings && pl->m_levelSettings->m_platformerMode) return;
    if (!pl->m_player1 || pl->m_player1->m_isDead) return;

    bool dual = pl->m_gameState.m_isDualMode && pl->m_player2;
    auto twoPlayerOf = [](auto* lay) -> bool {
        if constexpr (requires { lay->m_levelSettings->m_twoPlayerMode; }) {
            return lay->m_levelSettings && lay->m_levelSettings->m_twoPlayerMode;
        } else {
            return false;
        }
    };
    bool twoPlayer = twoPlayerOf(pl);
    int mask = (dual && !twoPlayer) ? 3 : (isPlayer1 ? 1 : 2);
    if ((mask & 2) && !pl->m_player2) return;

    captureRing(pl);
    if (g_overflow) return;

    int want = std::clamp(cky::fwBack, 0, MAX_BACK);
    for (int pi = 0; pi < 2; ++pi) {
        if (!(mask & (1 << pi))) continue;
        int avail = -1;
        for (int k = 0; k <= want && k < RING; ++k) {
            if (g_head[pi] < 0) break;
            int idx = ((g_head[pi] - k) % RING + RING) % RING;
            CkySimSnap const& e = g_ring[pi][idx];
            if (!e.valid || e.step + static_cast<uint32_t>(k) != cky::step) break;
            cky::copySim(g_back[pi][k], e);
            avail = k;
        }
        g_backAvail[pi] = avail;
        if (avail < 0) return;
    }

    g_req.on = true;
    g_req.mask = mask;
    g_req.step = cky::step;
}

CkyFwResult const& ckyFwResult() { return g_res; }

void ckyFwTestSound() { playJingle(true); }

void ckyFwSave() {
    auto m = Mod::get();
    m->setSavedValue<bool>("fw-on", cky::fwOn);
    m->setSavedValue<bool>("fw-show", cky::fwShow);
    m->setSavedValue<bool>("fw-jingle", cky::fwJingle);
    m->setSavedValue<int>("fw-ahead", cky::fwAhead);
    m->setSavedValue<int>("fw-back", cky::fwBack);
    m->setSavedValue<int>("fw-look", cky::fwLook);
    m->setSavedValue<int>("fw-perfect", cky::fwPerfect);
    m->setSavedValue<int>("fw-vol", cky::fwVol);
    m->setSavedValue<float>("fw-scale", cky::fwScale);
    m->setSavedValue<float>("fw-speed", cky::fwSpeed);
}

$on_mod(Loaded) {
    auto m = Mod::get();
    cky::fwOn = m->getSavedValue<bool>("fw-on", false);
    cky::fwShow = m->getSavedValue<bool>("fw-show", true);
    cky::fwJingle = m->getSavedValue<bool>("fw-jingle", true);
    cky::fwAhead = std::clamp(m->getSavedValue<int>("fw-ahead", 10), 1, MAX_AHEAD);
    cky::fwBack = std::clamp(m->getSavedValue<int>("fw-back", 10), 0, MAX_BACK);
    cky::fwLook = std::clamp(m->getSavedValue<int>("fw-look", 90), 20, 240);
    cky::fwPerfect = std::clamp(m->getSavedValue<int>("fw-perfect", 1), 1, 6);
    cky::fwVol = std::clamp(m->getSavedValue<int>("fw-vol", 100), 0, 200);
    cky::fwScale = std::clamp(m->getSavedValue<float>("fw-scale", 1.f), 0.5f, 2.5f);
    cky::fwSpeed = std::clamp(m->getSavedValue<float>("fw-speed", 0.5f), 0.25f, 2.f);
}

class $modify(CkyFwPlayLayer, PlayLayer) {
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!cky::fwOn) { g_jingle.on = false; g_req.on = false; return; }
        jingleTick(this);
        if (cky::trajCreating) return;
        runRequest(this);
    }

    void resetLevel() {
        resetAll();
        PlayLayer::resetLevel();
    }

    void onQuit() {
        resetAll();
        g_a = nullptr;
        g_b = nullptr;
        PlayLayer::onQuit();
    }
};

