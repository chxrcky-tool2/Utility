#include "state.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>

class $modify(CkyMacroHook, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (down && PlayLayer::get()) {
            cky::currentClicks++;
            cky::totalClicks++;
            if (cky::currentClicks > cky::highestClicks) cky::highestClicks = cky::currentClicks;
        }

        if (cky::recording && cky::armed && !cky::injecting) {
            cky::MacroInput input;
            input.frame = static_cast<uint32_t>(cky::frameClock + 0.5);
            input.isPress = down;
            input.playerButton = button | (isPlayer1 ? 0 : cky::P2_FLAG);
            cky::currentMacro.push_back(input);
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        // FIX: Added !cky::currentMacro.empty() to prevent crash on Play
        if (cky::playing && cky::armed && !cky::currentMacro.empty()) {
            double now = cky::frameClock;
            cky::injecting = true;
            
            while (cky::playbackCursor < cky::currentMacro.size()) {
                const auto& input = cky::currentMacro[cky::playbackCursor];
                if (static_cast<double>(input.frame) > now) break;
                
                cky::playbackCursor++;
                bool isP1 = (input.playerButton & cky::P2_FLAG) == 0;
                int btn = input.playerButton & 0xFF;
                
                GJBaseGameLayer::handleButton(input.isPress, btn, isP1);
            }
            cky::injecting = false;
            
            if (cky::playbackCursor >= cky::currentMacro.size()) {
                cky::playing = false;
                cky::armed = false;
            }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        
        // Advance the frame clock based on TPS
        cky::frameClock += 1.0 / cky::targetTps;
    }
};
