// real_fields.hpp — PlayerObject members saved/restored for the REAL players only
// (back step anchors and practice checkpoints, see player_state.hpp).
//
// They are NOT in traj_fields.hpp on purpose: that list feeds the Show Trajectory fake players, and
// xdBot (Zilko) skips m_isDashing for fake players. Real players need it, otherwise the dash state
// is lost after a checkpoint / back step. Same rule as xdBot's applyData: used only if the member
// exists in the SDK you build against and is plain data.
#pragma once
#define CKY_REAL_FIELDS(X) \
    X(m_isDashing) \
    X(m_holdingLeft) \
    X(m_holdingRight)

// END OF FILE (real_fields.hpp)
