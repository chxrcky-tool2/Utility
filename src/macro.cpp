#include "state.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>

class $modify(CkyMacroHook, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (cky::recording && cky::recArmed && !cky::injecting) {
            cky::MacroInput input;
            input.frame = static_cast<uint32_t>(cky::frameClock + 0.5);
            input.isPress = down;
            input.playerButton = button | (isPlayer1 ? 0 : cky::P2_FLAG);
            cky::macro.push_back(input);
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (cky::playing && cky::armed && !cky::macro.empty()) {
            double now = cky::frameClock;
            cky::injecting = true;
            while (cky::cursor < cky::macro.size()) {
                const auto& input = cky::macro[cky::cursor];
                if (static_cast<double>(input.frame) > now) break;
                cky::cursor++;
                GJBaseGameLayer::handleButton(input.isPress, input.playerButton & 0xFF, (input.playerButton & cky::P2_FLAG) == 0);
            }
            cky::injecting = false;
            if (cky::cursor >= cky::macro.size()) { cky::playing = false; cky::playArmed = false; }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        cky::frameClock += cky::clockInc();
    }
};
