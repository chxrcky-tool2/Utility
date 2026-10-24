#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <filesystem>

using namespace geode::prelude;

namespace cky {
    constexpr int P2_FLAG = 0x100;
    constexpr int MAX_SLOTS = 10;

    struct MacroInput {
        uint32_t frame;
        bool isPress;
        int playerButton;
    };

    inline bool recording = false, playing = false, injecting = false, frozen = false, armed = false, recArmed = false, playArmed = false, noclip = false, safeMode = false;
    inline bool tpsOn = true, speedOn = false, audioSpeed = true, fpsOn = false, hitboxes = false, hudOn = true, startPosOn = true, clickbotSounds = true, autoClicker = false;
    
    inline float speed = 1.f, tps = 240.f, macroTps = 240.f, fps = 60.f, targetTps = 240.f, autoClickerCps = 136.0f, clickAccumulator = 0.0f;
    inline int currentClicks = 0, highestClicks = 0, totalClicks = 0, slot = 1;
    
    inline std::vector<MacroInput> macro;
    inline size_t cursor = 0, playbackCursor = 0;
    inline double frameClock = 0.0;

    inline float effTps() { return tpsOn ? tps : 240.f; }
    inline float clockInc() { return 1.0 / effTps(); }
    
    inline float nextVal(std::vector<float> const& list, float cur, int dir) {
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i] >= cur) {
                int idx = std::clamp(static_cast<int>(i) + dir, 0, static_cast<int>(list.size()) - 1);
                return list[idx];
            }
        }
        return list.back();
    }

    inline std::vector<float> tpsList = {10, 20, 30, 60, 120, 240, 480, 1000, 2000, 5000, 10000, 20000, 24000};
    inline std::vector<float> speedList = {0.1f, 0.2f, 0.25f, 0.3f, 0.4f, 0.5f, 0.75f, 1.f, 1.5f, 2.f, 3.f, 4.f, 10.f};
    inline std::vector<float> fpsList = {30, 60, 120, 144, 240};
    inline std::vector<float> sizeList = {0.5f, 0.75f, 1.f, 1.25f, 1.5f, 2.f};

    bool save();
    bool load();
    bool slotExists();
    void clearMacro();
    void ckyNotify(std::string const& msg, bool success = true);
    
    void ckyBuildPanel(PlayLayer* pl);
    void ckyBuildHud(PlayLayer* pl);
    void ckyHudRefresh();
    void ckyBuildStartPos(PlayLayer* pl);
    void ckyApplyHudScale();
    void ckyOpenPanel(CCNode* host);
    void ckySetStartPosVisible(bool on);
    CCNode* ckyHudNode();
}
