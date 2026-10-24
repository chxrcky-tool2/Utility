#include "state.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <fstream>

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
        if (cky::playing && cky::armed && !cky::currentMacro.empty()) {
            double now = cky::frameClock;
            cky::injecting = true;
            while (cky::playbackCursor < cky::currentMacro.size()) {
                const auto& input = cky::currentMacro[cky::playbackCursor];
                if (static_cast<double>(input.frame) > now) break;
                cky::playbackCursor++;
                GJBaseGameLayer::handleButton(input.isPress, input.playerButton & 0xFF, (input.playerButton & cky::P2_FLAG) == 0);
            }
            cky::injecting = false;
            if (cky::playbackCursor >= cky::currentMacro.size()) { cky::playing = false; cky::armed = false; }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        cky::frameClock += 1.0 / cky::targetTps;
    }
};

namespace cky {
    void saveMacro(const std::string& name) {
        std::string path = Mod::get()->getSaveDir().string() + "/" + name + ".cky";
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return;
        file.write("CKY1", 4);
        uint32_t count = currentMacro.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& input : currentMacro) file.write(reinterpret_cast<const char*>(&input), sizeof(MacroInput));
    }
}
