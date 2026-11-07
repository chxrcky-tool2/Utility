// fixstate.hpp — small extra player state stored with every frame fix (CKY5 macros).
//
// A frame fix used to hold position, rotation and Y velocity. When a run drifts, those three can
// match while the game's *state* still differs (on ground or not, dashing or not, jump buffered
// or not), and the next step then goes a different way. This keeps a few of those flags plus the
// dash numbers, so a drift is detected on the exact step it starts and corrected.
//
// Every member is optional: if this SDK has no such member it is simply skipped (compiles away).
#pragma once
#include <cstdint>
#include <type_traits>

struct FixExtra {
    uint8_t flags = 0;
    double dashX = 0.0, dashY = 0.0, dashAngle = 0.0, dashStart = 0.0;
};

namespace cky {
    enum : uint8_t {
        FX_GROUND = 1, FX_UPSIDE = 2, FX_DASH = 4, FX_JBUF = 8, FX_SJBUF = 16, FX_SLOPE = 32
    };
    // flags that count as "the state differs" when compared against a recording
    constexpr uint8_t FX_COMPARE = FX_GROUND | FX_UPSIDE | FX_DASH | FX_JBUF;

#define CKY_FX_GETB(bit, f) if constexpr (requires { p->f; }) { if (p->f) e.flags |= (bit); }
#define CKY_FX_SETB(bit, f) if constexpr (requires { p->f = true; }) { p->f = ((e.flags & (bit)) != 0); }
#define CKY_FX_GETD(dst, f) if constexpr (requires { static_cast<double>(p->f); }) { e.dst = static_cast<double>(p->f); }
#define CKY_FX_SETD(src, f) if constexpr (requires { p->f = static_cast<std::remove_cvref_t<decltype(p->f)>>(e.src); }) { \
        p->f = static_cast<std::remove_cvref_t<decltype(p->f)>>(e.src); }

    template <class P>
    inline FixExtra captureExtra(P* p) {
        FixExtra e;
        if (!p) return e;
        CKY_FX_GETB(FX_GROUND, m_isOnGround)
        CKY_FX_GETB(FX_UPSIDE, m_isUpsideDown)
        CKY_FX_GETB(FX_DASH,   m_isDashing)
        CKY_FX_GETB(FX_JBUF,   m_jumpBuffered)
        CKY_FX_GETB(FX_SJBUF,  m_stateJumpBuffered)
        CKY_FX_GETB(FX_SLOPE,  m_isOnSlope)
        CKY_FX_GETD(dashX,     m_dashX)
        CKY_FX_GETD(dashY,     m_dashY)
        CKY_FX_GETD(dashAngle, m_dashAngle)
        CKY_FX_GETD(dashStart, m_dashStartTime)
        return e;
    }

    // Correction. Deliberately does NOT force m_isDashing / m_isUpsideDown / slope flags: setting
    // those without the object they belong to (dash orb, slope) is unsafe, so a mismatch there is
    // reported in the log instead. Ground + jump-buffer flags are safe to write. Dash numbers are
    // only written while the player really is dashing in both the recording and the live game.
    template <class P>
    inline void applyExtra(P* p, FixExtra const& e) {
        if (!p) return;
        CKY_FX_SETB(FX_GROUND, m_isOnGround)
        CKY_FX_SETB(FX_JBUF,   m_jumpBuffered)
        CKY_FX_SETB(FX_SJBUF,  m_stateJumpBuffered)
        bool liveDash = false;
        if constexpr (requires { p->m_isDashing; }) liveDash = static_cast<bool>(p->m_isDashing);
        if (liveDash && (e.flags & FX_DASH)) {
            CKY_FX_SETD(dashX,     m_dashX)
            CKY_FX_SETD(dashY,     m_dashY)
            CKY_FX_SETD(dashAngle, m_dashAngle)
            CKY_FX_SETD(dashStart, m_dashStartTime)
        }
    }

    // which of the compared flags differ (0 = same state)
    inline uint8_t extraMismatch(FixExtra const& live, FixExtra const& rec) {
        return static_cast<uint8_t>((live.flags ^ rec.flags) & FX_COMPARE);
    }
#undef CKY_FX_GETB
#undef CKY_FX_SETB
#undef CKY_FX_GETD
#undef CKY_FX_SETD
}

// END OF FILE (fixstate.hpp)
