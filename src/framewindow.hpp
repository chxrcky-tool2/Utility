// framewindow.hpp — Frame Window Calculator: settings + result shared by framewindow.cpp (the physics side)
// and imgui_ui.cpp (the label and the menu section).
#pragma once
#include <Geode/Geode.hpp>
#include <chrono>
#include <cmath>

struct CkyFwResult {
    bool valid = false;       // a calculation exists for the last click
    bool hit = false;         // the click that was made sits inside a surviving window
    bool truncated = false;   // the window reaches the edge of the tested range: real width is >= `width`
    bool incomplete = false;  // the time budget ran out; the result may be too optimistic
    bool perfect = false;     // hit, and the window is no wider than the "perfect" setting
    bool window = false;      // any surviving window was found (hit, or `next` away)
    bool hasNext = false;     // missed click: `next` is the nearest step offset that would have survived
    int  lo = 0, hi = 0;      // window, in physics steps relative to the click (inclusive)
    int  width = 0;           // hi - lo + 1
    int  next = 0;
    int  sims = 0;            // simulated branches this calculation
    float  stepHz = 240.f;    // physics steps per second the window is measured in
    double ms = 0.0;          // time the calculation took
    double stamp = 0.0;       // cky::fwNow() when it finished
    cocos2d::CCPoint anchor{ 0.f, 0.f };   // where the label goes, object-layer coordinates
};

namespace cky {
    inline bool  fwOn = false;      // master switch (off = nothing is captured or simulated)
    inline bool  fwShow = true;     // draw the label
    inline bool  fwJingle = true;   // Jingle Bells on a frame-perfect hit
    inline int   fwAhead = 10;      // physics steps after the click that are tested as click times (1-10)
    inline int   fwBack = 10;       // physics steps before the click that are tested (0-10)
    inline int   fwLook = 90;       // steps simulated after each tested click (20-240)
    inline int   fwPerfect = 1;     // "frame perfect" = window of at most this many steps (1-6)
    inline int   fwVol = 100;       // jingle volume, percent (0-200)
    inline float fwScale = 1.f;     // label size
    inline float fwHold = 2.5f;     // seconds the label stays once the count has finished
    inline float fwSpeed = 0.5f;    // count speed: 1.0x = 20 frames counted per second, 0.5x = 10 per second

    inline double fwNow() {
        using C = std::chrono::steady_clock;
        return std::chrono::duration<double>(C::now().time_since_epoch()).count();
    }
}

// Lowest of 60 / 120 / 240 FPS at which a window of `width` steps is GUARANTEED to be hit (0 = none of them).
// A frame at F FPS lands every stepHz / F steps, so the window must be at least that wide:
//   1 step -> 240 FW, 2-3 steps -> 120 FW, 4+ steps -> 60 FW   (at 240 steps/s)
inline int ckyFwTier(int width, float stepHz) {
    static constexpr int kT[3] = { 60, 120, 240 };
    for (int t : kT) {
        int need = static_cast<int>(std::ceil(stepHz / static_cast<float>(t) - 1e-4f));
        if (width >= std::max(1, need)) return t;
    }
    return 0;
}

void ckyFwCapture(GJBaseGameLayer* layer);                  // once per physics step (cheap ring snapshot)
void ckyFwOnPress(GJBaseGameLayer* layer, bool isPlayer1);  // a click reached the game: queue a calculation
CkyFwResult const& ckyFwResult();
void ckyFwSave();
void ckyFwTestSound();

// END OF FILE (framewindow.hpp)
