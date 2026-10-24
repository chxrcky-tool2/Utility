#include "state.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

// Save/Load logic moved here because state.cpp is deleted
namespace cky {
    bool save() {
        std::string path = Mod::get()->getSaveDir().string() + "/slot_" + std::to_string(slot) + ".cky";
        std::ofstream f(path, std::ios::binary);
        if (!f.is_open()) return false;
        f.write("CKY1", 4);
        uint32_t count = macro.size();
        f.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (auto& i : macro) f.write(reinterpret_cast<const char*>(&i), sizeof(MacroInput));
        return true;
    }

    bool load() {
        std::string path = Mod::get()->getSaveDir().string() + "/slot_" + std::to_string(slot) + ".cky";
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return false;
        char h[4]; f.read(h, 4);
        if (std::string(h, 4) != "CKY1") return false;
        uint32_t count; f.read(reinterpret_cast<char*>(&count), sizeof(count));
        macro.clear(); macro.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            MacroInput in; f.read(reinterpret_cast<char*>(&in), sizeof(MacroInput));
            macro.push_back(in);
        }
        cursor = 0;
        return true;
    }

    bool slotExists() {
        std::string path = Mod::get()->getSaveDir().string() + "/slot_" + std::to_string(slot) + ".cky";
        return std::filesystem::exists(path);
    }

    void clearMacro() { macro.clear(); cursor = 0; }
    void rewindTo(Cp const& cp) { cursor = cp.cursor; frameClock = cp.frame; }
    void trackCheckpoints(int count) {
        if (count > static_cast<int>(ckpts.size())) {
            Cp cp; cp.frame = frameClock; cp.cursor = cursor; cp.valid = true;
            ckpts.push_back(cp);
        } else if (count < static_cast<int>(ckpts.size())) {
            ckpts.resize(count);
        }
    }
    void ckyNotify(std::string const& msg, bool success) {
        Notification::create(msg, success ? NotificationIcon::Success : NotificationIcon::Error)->show();
    }
}

class $modify(CkyGameLayer, GJBaseGameLayer) {
    float getModifiedDelta(float dt) {
        if ((cky::effTps() == 240.f && !cky::frozen) || !PlayLayer::get()) return GJBaseGameLayer::getModifiedDelta(dt);
        float modifier = cky::effTps();
        float total = dt + m_extraDelta;
        int steps = static_cast<int>(total / modifier + 0.0001f);
        float result = steps * modifier;
        m_extraDelta = total - result;
        return result;
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (auto pl = PlayLayer::get()) {
            cky::trackCheckpoints(pl->m_checkpointArray ? pl->m_checkpointArray->count() : 0);
            if (cky::playing && cky::playArmed && !cky::macro.empty()) {
                cky::injecting = true;
                while (cky::cursor < cky::macro.size() && cky::macro[cky::cursor].frame <= cky::frameClock) {
                    auto& in = cky::macro[cky::cursor++];
                    GJBaseGameLayer::handleButton(in.isPress, in.playerButton & 0xFF, (in.playerButton & cky::P2_FLAG) == 0);
                }
                cky::injecting = false;
                if (cky::cursor >= cky::macro.size()) { cky::playing = false; cky::ckyNotify("Playback Finished"); }
            }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        cky::frameClock += cky::clockInc();
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (PlayLayer::get()) {
            if (cky::playing && !cky::injecting) return;
            if (cky::recording && cky::recArmed && !cky::injecting) {
                cky::macro.push_back({(uint32_t)cky::frameClock, down, button | (isPlayer1 ? 0 : cky::P2_FLAG)});
            }
            if (down) { cky::currentClicks++; cky::totalClicks++; if (cky::currentClicks > cky::highestClicks) cky::highestClicks = cky::currentClicks; }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

class $modify(CkyPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        cky::frameClock = 0; cky::cursor = 0; cky::ckpts.clear(); cky::recArmed = cky::recording; cky::playArmed = cky::playing;
        cky::ckyBuildHud(this); cky::ckyBuildStartPos(this); cky::ckyOpenPanel(this->m_uiLayer);
    }
    void resetLevel() {
        PlayLayer::resetLevel();
        if (cky::recording && cky::recArmed && m_isPracticeMode && !cky::ckpts.empty()) cky::rewindTo(cky::ckpts.back());
        else { cky::frameClock = 0; cky::cursor = 0; cky::ckpts.clear(); cky::macro.clear(); }
        if (cky::recording) cky::recArmed = true;
        if (cky::playing) cky::playArmed = true;
    }
    void destroyPlayer(PlayerObject* p, GameObject* o) {
        if (cky::noclip && o != m_anticheatSpike) return;
        PlayLayer::destroyPlayer(p, o);
    }
};
