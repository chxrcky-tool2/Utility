#include "state.hpp"
#include <string_view>

#define CKY_SF_EXTRA(X) \
    X(m_lastPosition) \
    X(m_xVelocity)

CKY_SF_EXTRA(CKY_DEF_OK)

namespace {
    constexpr size_t SF_MAX = 4096;
    constexpr size_t SF_MAX_BYTES = 24u * 1024u * 1024u;

    constexpr bool sfSkip(std::string_view n) {
        constexpr std::string_view k[] = {
            "m_isDead", "m_isLocked", "m_controlsDisabled", "m_inputsLocked", "m_isSecondPlayer",
            "m_iconRequestID", "m_isOutOfBounds", "m_ignoreDamage", "m_isBeingSpawnedByDualPortal",
            "m_checkpointTimeout", "m_lastCheckpointTime", "m_canPlaceCheckpoint",
            "m_shouldTryPlacingCheckpoint", "m_onFlyCheckpointTries", "m_quickCheckpointMode"
        };
        for (auto const& s : k) {
            if (s == n) return true;
        }
        return false;
    }

    template <bool Portable, class T>
    constexpr bool sfKeep(std::string_view name) {
        if constexpr (Portable) return !std::is_pointer_v<T> && !sfSkip(name);
        else return true;
    }

    template <bool Portable>
    bool sfPack(PlayerObject* p, uint8_t* out, size_t cap, size_t& n) {
        size_t off = 0;
        bool over = false;
        auto run = [&](auto* d) {
#define CKY_SF_PACK(f) if constexpr (CKY_PS_OK(f)) { \
            using T = std::remove_cvref_t<decltype(d->f)>; \
            if constexpr (sfKeep<Portable, T>(#f)) { \
                if (off + sizeof(T) > cap) { \
                    over = true; \
                } else { \
                    T tmp = d->f; \
                    std::memcpy(out + off, &tmp, sizeof(T)); \
                    off += sizeof(T); \
                } \
            } }
            CKY_TRAJ_FIELDS(CKY_SF_PACK)
            CKY_REAL_FIELDS(CKY_SF_PACK)
            CKY_SF_EXTRA(CKY_SF_PACK)
#undef CKY_SF_PACK
        };
        run(p);
        n = off;
        return !over;
    }

    template <bool Portable>
    void sfUnpack(PlayerObject* p, uint8_t const* in, size_t n) {
        size_t off = 0;
        auto run = [&](auto* d) {
#define CKY_SF_UNPACK(f) if constexpr (CKY_PS_OK(f)) { \
            using T = std::remove_cvref_t<decltype(d->f)>; \
            if constexpr (sfKeep<Portable, T>(#f)) { \
                T tmp; \
                if (off + sizeof(T) <= n) { \
                    std::memcpy(&tmp, in + off, sizeof(T)); \
                    d->f = tmp; \
                } \
                off += sizeof(T); \
            } }
            CKY_TRAJ_FIELDS(CKY_SF_UNPACK)
            CKY_REAL_FIELDS(CKY_SF_UNPACK)
            CKY_SF_EXTRA(CKY_SF_UNPACK)
#undef CKY_SF_UNPACK
        };
        run(p);
    }

    template <bool Portable>
    char const* sfFirstDiff(uint8_t const* a, uint8_t const* b, size_t n) {
        char const* name = nullptr;
        size_t off = 0;
        auto run = [&](auto* d) {
#define CKY_SF_DIFF(f) if constexpr (CKY_PS_OK(f)) { \
            using T = std::remove_cvref_t<decltype(d->f)>; \
            if constexpr (sfKeep<Portable, T>(#f)) { \
                if (!name && off + sizeof(T) <= n && std::memcmp(a + off, b + off, sizeof(T)) != 0) name = #f; \
                off += sizeof(T); \
            } }
            CKY_TRAJ_FIELDS(CKY_SF_DIFF)
            CKY_REAL_FIELDS(CKY_SF_DIFF)
            CKY_SF_EXTRA(CKY_SF_DIFF)
#undef CKY_SF_DIFF
        };
        run(static_cast<PlayerObject*>(nullptr));
        return name ? name : "(position / rotation)";
    }

    template <bool Portable>
    uint32_t sfHashImpl() {
        uint32_t h = 2166136261u;
        auto mix = [&](uint8_t b) {
            h ^= b;
            h *= 16777619u;
        };
        auto run = [&](auto* d) {
#define CKY_SF_HASH(f) if constexpr (CKY_PS_OK(f)) { \
            using T = std::remove_cvref_t<decltype(d->f)>; \
            if constexpr (sfKeep<Portable, T>(#f)) { \
                for (char const* c = #f; *c; ++c) mix(static_cast<uint8_t>(*c)); \
                mix(static_cast<uint8_t>(sizeof(T))); \
            } }
            CKY_TRAJ_FIELDS(CKY_SF_HASH)
            CKY_REAL_FIELDS(CKY_SF_HASH)
            CKY_SF_EXTRA(CKY_SF_HASH)
#undef CKY_SF_HASH
        };
        run(static_cast<PlayerObject*>(nullptr));
        return h;
    }

    struct Slot {
        uint32_t step = 0;
        uint8_t phase = 0;
        uint8_t sub = 0;
        uint8_t dual = 0;
        float px[2] = { 0.f, 0.f };
        float py[2] = { 0.f, 0.f };
        float rot[2] = { 0.f, 0.f };
        float timeWarp = 1.f;
        float extraDelta = 0.f;
    };

    std::vector<Slot> g_slots;
    std::vector<uint8_t> g_data;
    size_t g_cap = 0;
    size_t g_head = 0;
    size_t g_count = 0;
    size_t g_per = 0;
    uint8_t g_scratch[2][SF_MAX];
    uint8_t g_sub = 0;
    uint32_t g_subStep = 0xFFFFFFFFu;
    uint8_t g_vsub = 0;
    uint32_t g_vsubStep = 0xFFFFFFFFu;
    bool g_packFailed = false;

    size_t physIndex(size_t i) {
        return (g_head + g_cap - 1 - i) % g_cap;
    }

    Slot& slotAt(size_t i) {
        return g_slots[physIndex(i)];
    }

    uint8_t* dataAt(size_t phys) {
        return g_data.data() + phys * 2 * g_per;
    }

    void popNewest() {
        if (g_count == 0) return;
        g_head = (g_head + g_cap - 1) % g_cap;
        --g_count;
    }

    void ensure(size_t n) {
        size_t want = static_cast<size_t>(std::clamp(cky::sfDepth, 30, 960)) * 3;
        size_t byCap = SF_MAX_BYTES / (2 * n);
        if (want > byCap) want = std::max<size_t>(byCap, 16);
        if (n == g_per && want == g_cap && !g_slots.empty()) return;
        g_per = n;
        g_cap = want;
        g_slots.assign(g_cap, Slot{});
        g_data.assign(g_cap * 2 * g_per, 0);
        g_head = 0;
        g_count = 0;
    }

    void dropBeginTail(uint32_t step) {
        while (g_count > 0 && slotAt(0).step > step) popNewest();
        size_t i = 0;
        bool found = false;
        while (i < g_count && slotAt(i).step == step) {
            if (slotAt(i).phase == 0) {
                found = true;
                break;
            }
            ++i;
        }
        if (found) {
            for (size_t k = 0; k <= i; ++k) popNewest();
        }
    }

    void dropInputTail(uint32_t step) {
        while (g_count > 0 && slotAt(0).step > step) popNewest();
        while (g_count > 0 && slotAt(0).step == step && slotAt(0).phase == 1) popNewest();
    }

    void dropHalfTail(uint32_t step, uint8_t sub) {
        while (g_count > 0 && slotAt(0).step > step) popNewest();
        while (g_count > 0 && slotAt(0).step == step && slotAt(0).phase == 2 && slotAt(0).sub >= sub) popNewest();
    }

    bool findSlot(uint32_t step, uint8_t phase, uint8_t sub, size_t& physOut) {
        for (size_t i = 0; i < g_count; ++i) {
            size_t pi = physIndex(i);
            Slot const& s = g_slots[pi];
            if (s.step < step) return false;
            if (s.step == step && s.phase == phase && s.sub == sub) {
                physOut = pi;
                return true;
            }
        }
        return false;
    }

    void applyPose(PlayerObject* p, float x, float y, float r) {
        p->setPosition(CCPoint(x, y));
        p->setRotation(r);
    }

    bool verifyAt(GJBaseGameLayer* l, uint32_t step, uint8_t phase, uint8_t sub, bool apply, char const* tag) {
        size_t pi = 0;
        if (!l || !l->m_player1 || l->m_player1->m_isDead) return false;
        if (!findSlot(step, phase, sub, pi)) return false;
        Slot const& s = g_slots[pi];
        uint8_t const* d = dataAt(pi);
        size_t n1 = 0;
        if (!sfPack<false>(l->m_player1, g_scratch[0], SF_MAX, n1) || n1 != g_per) return false;
        bool bad1 = std::memcmp(g_scratch[0], d, g_per) != 0
            || l->m_player1->getPositionX() != s.px[0]
            || l->m_player1->getPositionY() != s.py[0]
            || l->m_player1->getRotation() != s.rot[0];
        bool useP2 = s.dual && l->m_gameState.m_isDualMode && l->m_player2 && !l->m_player2->m_isDead;
        bool bad2 = false;
        if (useP2) {
            size_t n2 = 0;
            if (sfPack<false>(l->m_player2, g_scratch[1], SF_MAX, n2) && n2 == g_per) {
                bad2 = std::memcmp(g_scratch[1], d + g_per, g_per) != 0
                    || l->m_player2->getPositionX() != s.px[1]
                    || l->m_player2->getPositionY() != s.py[1]
                    || l->m_player2->getRotation() != s.rot[1];
            }
        }
        ++cky::sfRingChecks;
        if (!bad1 && !bad2) return false;
        ++cky::sfRingMisses;
        char const* field = bad1 ? sfFirstDiff<false>(g_scratch[0], d, g_per)
                                 : sfFirstDiff<false>(g_scratch[1], d + g_per, g_per);
        if (cky::sfRingMisses <= 8 || (cky::sfRingMisses % 64) == 0) {
            log::warn("[cky] subframe {}: step {} phase {} sub {} drifted (player {}, first field {}, timeWarp {:.4f} vs {:.4f}){}",
                tag, step, static_cast<int>(phase), static_cast<int>(sub), bad1 ? 1 : 2, field,
                l->m_gameState.m_timeWarp, s.timeWarp, apply ? " - snapshot restored" : "");
        }
        if (cky::sfRingMisses == 1) {
            ckyOverlayShow(fmt::format("Sub-frame restored @ step {}", step), false);
        }
        if (apply) {
            if (bad1) {
                sfUnpack<false>(l->m_player1, d, g_per);
                applyPose(l->m_player1, s.px[0], s.py[0], s.rot[0]);
            }
            if (bad2) {
                sfUnpack<false>(l->m_player2, d + g_per, g_per);
                applyPose(l->m_player2, s.px[1], s.py[1], s.rot[1]);
            }
        }
        return true;
    }
}

void ckySfClear(bool release) {
    g_count = 0;
    g_head = 0;
    g_subStep = 0xFFFFFFFFu;
    g_vsubStep = 0xFFFFFFFFu;
    g_packFailed = false;
    cky::sfRingChecks = 0;
    cky::sfRingMisses = 0;
    cky::sfBlobChecks = 0;
    cky::sfBlobMisses = 0;
    if (release) {
        std::vector<Slot>().swap(g_slots);
        std::vector<uint8_t>().swap(g_data);
        g_cap = 0;
        g_per = 0;
    }
}

size_t ckySfCount() {
    return g_count;
}

size_t ckySfBytes() {
    return g_data.size() + g_slots.size() * sizeof(Slot);
}

uint32_t ckySfLayoutHash() {
    static uint32_t const h = sfHashImpl<true>();
    return h;
}

void ckySfCapture(GJBaseGameLayer* l, CkySfPhase ph) {
    if (!cky::sfOn || !cky::backStepOn || !l) return;
    if (cky::resimming || cky::trajCreating || cky::effTps() > 480.f) return;
    PlayerObject* p1 = l->m_player1;
    if (!p1 || p1->m_isDead) return;

    size_t n1 = 0;
    if (!sfPack<false>(p1, g_scratch[0], SF_MAX, n1) || n1 == 0) {
        if (!g_packFailed) {
            g_packFailed = true;
            log::error("[cky] subframe: player state does not fit {} bytes or is empty, snapshots disabled", SF_MAX);
        }
        return;
    }
    bool dual = l->m_gameState.m_isDualMode && l->m_player2 != nullptr;
    if (dual) {
        size_t n2 = 0;
        if (!sfPack<false>(l->m_player2, g_scratch[1], SF_MAX, n2) || n2 != n1) dual = false;
    }
    ensure(n1);

    uint32_t step = cky::step;
    uint8_t sub = 0;
    if (ph == CkySfPhase::Begin) {
        dropBeginTail(step);
    } else if (ph == CkySfPhase::Input) {
        dropInputTail(step);
    } else {
        if (g_subStep != step) {
            g_subStep = step;
            g_sub = 0;
        }
        sub = g_sub;
        if (g_sub < 250) ++g_sub;
        dropHalfTail(step, sub);
    }

    size_t pi = g_head;
    Slot& s = g_slots[pi];
    s.step = step;
    s.phase = static_cast<uint8_t>(ph);
    s.sub = sub;
    s.dual = dual ? 1 : 0;
    s.px[0] = p1->getPositionX();
    s.py[0] = p1->getPositionY();
    s.rot[0] = p1->getRotation();
    if (dual) {
        s.px[1] = l->m_player2->getPositionX();
        s.py[1] = l->m_player2->getPositionY();
        s.rot[1] = l->m_player2->getRotation();
    }
    s.timeWarp = l->m_gameState.m_timeWarp;
    s.extraDelta = l->m_extraDelta;

    uint8_t* dst = dataAt(pi);
    std::memcpy(dst, g_scratch[0], g_per);
    if (dual) std::memcpy(dst + g_per, g_scratch[1], g_per);

    g_head = (g_head + 1) % g_cap;
    if (g_count < g_cap) ++g_count;
}

void ckySfResimStep(GJBaseGameLayer* l, uint32_t step, bool half) {
    if (!cky::sfOn || !l || g_count == 0) return;
    uint8_t sub = 0;
    if (half) {
        if (g_vsubStep != step) {
            g_vsubStep = step;
            g_vsub = 0;
        }
        sub = g_vsub;
        if (g_vsub < 250) ++g_vsub;
        verifyAt(l, step, 2, sub, true, "resim");
    } else {
        verifyAt(l, step, 0, 0, true, "resim");
    }
}

void ckySfSettle(GJBaseGameLayer* l, uint32_t step) {
    if (!cky::sfOn || !l) return;
    g_vsubStep = 0xFFFFFFFFu;
    bool drift = verifyAt(l, step, 0, 0, true, "backstep");
    size_t pi = 0;
    bool had = findSlot(step, 0, 0, pi);
    log::info("[cky] subframe: back step settled on step {} ({})", step,
        !had ? "no snapshot for this step" : (drift ? "state restored from snapshot" : "state matches snapshot"));
    while (g_count > 0 && (slotAt(0).step > step || (slotAt(0).step == step && slotAt(0).phase != 0))) popNewest();
    g_subStep = 0xFFFFFFFFu;
}

CkySfBlob ckySfMakeBlob(GJBaseGameLayer* l, uint32_t frame) {
    CkySfBlob b;
    b.frame = frame;
    if (!l || !l->m_player1 || l->m_player1->m_isDead) return b;
    size_t n1 = 0;
    if (!sfPack<true>(l->m_player1, g_scratch[0], SF_MAX, n1) || n1 == 0) return b;
    size_t n2 = 0;
    bool dual = l->m_gameState.m_isDualMode && l->m_player2 && !l->m_player2->m_isDead
        && sfPack<true>(l->m_player2, g_scratch[1], SF_MAX, n2) && n2 == n1;
    b.n[0] = static_cast<uint16_t>(n1);
    b.px[0] = l->m_player1->getPositionX();
    b.py[0] = l->m_player1->getPositionY();
    b.rot[0] = l->m_player1->getRotation();
    b.bytes.assign(g_scratch[0], g_scratch[0] + n1);
    if (dual) {
        b.dual = 1;
        b.n[1] = static_cast<uint16_t>(n2);
        b.px[1] = l->m_player2->getPositionX();
        b.py[1] = l->m_player2->getPositionY();
        b.rot[1] = l->m_player2->getRotation();
        b.bytes.insert(b.bytes.end(), g_scratch[1], g_scratch[1] + n2);
    }
    return b;
}

bool ckySfApplyBlob(GJBaseGameLayer* l, CkySfBlob const& b) {
    if (!cky::sfOn || !l || b.n[0] == 0) return false;
    size_t need = static_cast<size_t>(b.n[0]) + (b.dual ? static_cast<size_t>(b.n[1]) : 0u);
    if (b.bytes.size() < need) return false;

    ++cky::sfBlobChecks;
    bool corrected = false;
    char const* firstField = nullptr;
    int firstPlayer = 0;

    auto one = [&](PlayerObject* p, int pi) {
        if (!p || p->m_isDead) return;
        uint8_t const* data = b.bytes.data() + (pi == 0 ? 0u : static_cast<size_t>(b.n[0]));
        size_t n = 0;
        if (!sfPack<true>(p, g_scratch[pi], SF_MAX, n) || n != b.n[pi]) return;
        bool same = std::memcmp(g_scratch[pi], data, n) == 0
            && p->getPositionX() == b.px[pi]
            && p->getPositionY() == b.py[pi]
            && p->getRotation() == b.rot[pi];
        if (same) return;
        if (!corrected) {
            firstField = sfFirstDiff<true>(g_scratch[pi], data, n);
            firstPlayer = pi + 1;
        }
        corrected = true;
        sfUnpack<true>(p, data, n);
        applyPose(p, b.px[pi], b.py[pi], b.rot[pi]);
    };

    one(l->m_player1, 0);
    if (b.dual && l->m_gameState.m_isDualMode) one(l->m_player2, 1);

    if (corrected) {
        ++cky::sfBlobMisses;
        if (cky::sfBlobMisses <= 8 || (cky::sfBlobMisses % 64) == 0) {
            log::warn("[cky] subframe: playback drift at frame {} (player {}, first field {}) - macro snapshot applied",
                b.frame, firstPlayer, firstField);
        }
        if (cky::sfBlobMisses == 1) {
            ckyOverlayShow(fmt::format("Sub-frame corrected @ frame {}", b.frame), false);
        }
    }
    return corrected;
}
