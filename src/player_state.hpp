// player_state.hpp — full PlayerObject state snapshot / restore ("practice fixes").
//
// Idea and member list: xdBot by Zilko (PlayerPracticeFixes::saveData / applyData), used with his
// permission. The game's own CheckpointObject only restores part of a player; things like dash,
// slope, buffered-jump and collision state are NOT restored, so a rewound or resumed run can
// differ from the original. xdBot re-applies those members after loading a checkpoint; this does
// the same for the checkpoints taken by the back stepper and by practice mode.
//
// Differences from xdBot: the members live in one X-macro list (traj_fields.hpp, shared with the
// trajectory) and are packed into a byte buffer, so one snapshot is a single small allocation.
// A member is only used if it exists in the SDK you build against and is plain data (trivially
// copyable), so renamed/removed members are skipped instead of breaking the build.
#pragma once
#include "traj_fields.hpp"
#include "real_fields.hpp" // dash / holding state, real players only

#include <Geode/Geode.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

struct CkyPlayerState {
    std::vector<uint8_t> bytes;
    cocos2d::CCPoint pos{ 0.f, 0.f };
    float rot = 0.f;
    bool valid = false;
};

// member is usable: exists, plain data, default constructible and assignable (bit-fields included)
#define CKY_PS_OK(f) requires { \
    d->f; \
    requires std::is_trivially_copyable_v<std::remove_cvref_t<decltype(d->f)>>; \
    requires std::is_default_constructible_v<std::remove_cvref_t<decltype(d->f)>>; \
    d->f = d->f; }

namespace cky {
    template <class T>
    inline void psPut(std::vector<uint8_t>& out, T const& v) {
        auto const* b = reinterpret_cast<uint8_t const*>(&v);
        out.insert(out.end(), b, b + sizeof(T));
    }

    inline CkyPlayerState capturePlayer(PlayerObject* p) {
        CkyPlayerState s;
        if (!p) return s;
        s.bytes.reserve(2048);
        auto run = [&](auto* d) {
#define CKY_PS_CAP(f) if constexpr (CKY_PS_OK(f)) { psPut(s.bytes, d->f); }
            CKY_TRAJ_FIELDS(CKY_PS_CAP)
            CKY_REAL_FIELDS(CKY_PS_CAP)
#undef CKY_PS_CAP
        };
        run(p);
        s.pos = p->getPosition();
        s.rot = p->getRotation();
        s.valid = true;
        return s;
    }

    inline void restorePlayer(PlayerObject* p, CkyPlayerState const& s) {
        if (!p || !s.valid) return;
        auto run = [&](auto* d) {
            size_t off = 0;
#define CKY_PS_PUT(f) if constexpr (CKY_PS_OK(f)) { \
                std::remove_cvref_t<decltype(d->f)> tmp; \
                if (off + sizeof(tmp) <= s.bytes.size()) { \
                    std::memcpy(&tmp, s.bytes.data() + off, sizeof(tmp)); \
                    d->f = tmp; \
                } \
                off += sizeof(tmp); \
            }
            CKY_TRAJ_FIELDS(CKY_PS_PUT)
            CKY_REAL_FIELDS(CKY_PS_PUT)
#undef CKY_PS_PUT
        };
        run(p);
        p->setPosition(s.pos);
        p->setRotation(s.rot);
    }
}

// ---- allocation-free snapshot (Frame Window Calculator) ----------------------------------------
// Same member list as the trajectory fake players (CKY_TRAJ_FIELDS: no m_isDashing, like xdBot's fake
// players), but written into a fixed buffer, so taking one every physics step never allocates.
inline constexpr size_t CKY_SIM_BYTES = 3072;

struct CkySimSnap {
    std::array<uint8_t, CKY_SIM_BYTES> b;   // only [0, n) is meaningful
    uint16_t n = 0;
    cocos2d::CCPoint pos{ 0.f, 0.f };
    float rot = 0.f;
    uint32_t step = 0;                       // cky::step this snapshot was taken on
    bool valid = false;
};

namespace cky {
    template <class T>
    inline void simPut(CkySimSnap& s, size_t& off, bool& over, T const& v) {
        if (off + sizeof(T) > s.b.size()) { over = true; return; }
        std::memcpy(s.b.data() + off, &v, sizeof(T));
        off += sizeof(T);
    }

    // false = the member list no longer fits CKY_SIM_BYTES (raise it)
    inline bool captureSim(CkySimSnap& s, PlayerObject* p) {
        s.valid = false;
        s.n = 0;
        if (!p) return false;
        size_t off = 0;
        bool over = false;
        auto run = [&](auto* d) {
#define CKY_SIM_CAP(f) if constexpr (CKY_PS_OK(f)) { simPut(s, off, over, d->f); }
            CKY_TRAJ_FIELDS(CKY_SIM_CAP)
#undef CKY_SIM_CAP
        };
        run(p);
        if (over) return false;
        s.n = static_cast<uint16_t>(off);
        s.pos = p->getPosition();
        s.rot = p->getRotation();
        s.valid = true;
        return true;
    }

    inline void restoreSim(PlayerObject* p, CkySimSnap const& s) {
        if (!p || !s.valid) return;
        auto run = [&](auto* d) {
            size_t off = 0;
#define CKY_SIM_PUT(f) if constexpr (CKY_PS_OK(f)) { \
                std::remove_cvref_t<decltype(d->f)> tmp; \
                if (off + sizeof(tmp) <= s.n) { \
                    std::memcpy(&tmp, s.b.data() + off, sizeof(tmp)); \
                    d->f = tmp; \
                } \
                off += sizeof(tmp); \
            }
            CKY_TRAJ_FIELDS(CKY_SIM_PUT)
#undef CKY_SIM_PUT
        };
        run(p);
        p->setPosition(s.pos);
        p->setRotation(s.rot);
    }

    // copies only the used part of the buffer
    inline void copySim(CkySimSnap& dst, CkySimSnap const& src) {
        dst.valid = src.valid;
        dst.n = src.n;
        dst.pos = src.pos;
        dst.rot = src.rot;
        dst.step = src.step;
        if (src.n) std::memcpy(dst.b.data(), src.b.data(), src.n);
    }
}

// END OF FILE (player_state.hpp)
