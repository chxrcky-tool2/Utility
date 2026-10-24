#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <string>

namespace cky {
    constexpr int P2_FLAG = 0x100;

    struct MacroInput {
        uint32_t frame;
        bool isPress;
        int playerButton; 
    };

    struct CheckpointData {
        uint32_t frame;
        size_t cursor;
    };

    inline bool recording = false;
    inline bool playing = false;
    inline bool injecting = false;
    inline bool frozen = false;
    inline bool noclip = false;
    inline bool safeMode = false;
    inline bool armed = false;

    inline bool showOverlay = true;
    inline bool autoClicker = false;
    inline bool clickbotSounds = true;
    inline float speedhack = 1.f;
    inline float targetTps = 240.f;
    inline float autoClickerCps = 136.0f;
    inline float clickAccumulator = 0.0f;

    inline int currentClicks = 0;
    inline int highestClicks = 0;
    inline int totalClicks = 0;

    inline std::vector<MacroInput> currentMacro;
    inline size_t playbackCursor = 0;
    inline double frameClock = 0.0;
    inline std::vector<CheckpointData> checkpoints;
    inline int lastCheckpointCount = 0;
}
