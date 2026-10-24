#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace cky {
    constexpr uint32_t MAGIC1 = 0x31594B43; // "CKY1"
    constexpr int P2_FLAG = 0x100;
    constexpr int MAX_SLOTS = 10;

    struct MacroInput {
        uint32_t frame;
        bool isPress;
        int playerButton;
    };

    struct Cp {
        uint32_t frame = 0;
        size_t cursor = 0;
        bool valid = false;
    };

    // Core State
    inline bool recording = false;
    inline bool playing = false;
    inline bool injecting = false;
    inline bool frozen = false;
    inline int pending = 0;
    inline bool dirty = false;
    inline bool noclip = false;
    inline bool safeMode = false;
    inline bool recArmed = false;
    inline bool playArmed = false;

    // Toggles
    inline bool tpsOn = true;
    inline bool speedOn = false;
    inline bool audioSpeed = true;
    inline bool fpsOn = false;
    inline bool hitboxes = false;
    inline bool hudOn = true;
    inline bool startPosOn = true;
    inline bool ckptTelemetry = true;
    inline bool ncShowAcc = true;
    inline bool ncShowDeaths = true;

    // Values
    inline float speed = 1.f;
    inline float tps = 240.f;
    inline float macroTps = 240.f;
    inline float fps = 60.f;
    inline float hudScale = 1.f;
    inline float iconScale = 1.f;
    inline float actualTps = 240.f;

    // Telemetry
    inline int currentClicks = 0;
    inline int highestClicks = 0;
    inline int totalClicks = 0;
    inline int slot = 1;

    // Noclip Tracking
    inline int ncFrames = 0, ncBad = 0, ncDeaths = 0;
    inline bool ncBlocked = false, ncPrevBad = false;
    inline int measSteps = 0;
    inline float measTime = 0.f;

    // Data
    inline std::vector<MacroInput> macro;
    inline size_t cursor = 0;
    inline double frameClock = 0.0;
    inline uint32_t step = 0;
    inline int hudTick = 0;

    inline std::vector<Cp> ckpts;
    inline bool cpInit = false;

    // Helpers
    inline float effTps() { return tpsOn ? tps : 240.f; }
    inline float clockInc() { return 1.0 / effTps(); }
    inline bool cheating() { return noclip || speedOn || tpsOn; }
    inline void ncReset() { ncFrames = 0; ncBad = 0; ncDeaths = 0; ncBlocked = false; ncPrevBad = false; }
    inline void resetMeasure() { measSteps = 0; measTime = 0.f; actualTps = effTps(); }

    inline float nextVal(std::vector<float> const& list, float cur, int dir) {
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i] >= cur) {
                int idx = std::clamp(static_cast<int>(i) + dir, 0, static_cast<int>(list.size()) - 1);
                return list[idx];
            }
        }
        return list.back();
    }

    // Lists
    inline std::vector<float> tpsList = {10, 20, 30, 60, 120, 240, 480, 1000, 2000, 5000, 10000, 20000};
    inline std::vector<float> speedList = {0.1f, 0.2f, 0.25f, 0.3f, 0.4f, 0.5f, 0.75f, 1.f, 1.5f, 2.f, 3.f, 4.f, 10.f};
    inline std::vector<float> fpsList = {30, 60, 120, 144, 240};
    inline std::vector<float> sizeList = {0.5f, 0.75f, 1.f, 1.25f, 1.5f, 2.f};

    // Functions
    bool save();
    bool load();
    bool slotExists();
    void clearMacro();
    void rewindTo(Cp const& cp);
    void trackCheckpoints(int count);
    
    // Forward declarations for UI
    void ckyBuildHud(PlayLayer* pl);
    void ckyHudRefresh();
    void ckyBuildStartPos(PlayLayer* pl);
    void ckyApplyAudio();
    void ckyApplyFps();
    void ckyApplyHitboxes();
    void ckyApplyHudScale();
    void ckyOpenPanel(CCNode* host);
    void ckySetStartPosVisible(bool on);
    void ckyNotify(std::string const& msg, bool success = true);
    CCNode* ckyHudNode();
}
