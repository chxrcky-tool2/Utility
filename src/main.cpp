#include "state.hpp"
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

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
        for (uint32_t i = 0; i < count; ++i) { MacroInput in; f.read(reinterpret_cast<char*>(&in), sizeof(MacroInput)); macro.push_back(in); }
        cursor = 0;
        return true;
    }
    bool slotExists() {
        std::string path = Mod::get()->getSaveDir().string() + "/slot_" + std::to_string(slot) + ".cky";
        return std::filesystem::exists(path);
    }
    void clearMacro() { macro.clear(); cursor = 0; }
    void ckyNotify(std::string const& msg, bool success) { Notification::create(msg, success ? NotificationIcon::Success : NotificationIcon::Error)->show(); }
    void ckyApplyHudScale() {}
}

class $modify(CkyPlayLayer, PlayLayer) {
    // 1. Reset counters when entering the level
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        
        cky::frameClock = 0.0; 
        cky::playbackCursor = 0; 
        cky::armed = false;
        cky::currentClicks = 0; // Reset current
        cky::highestClicks = 0; // Reset highest (change this if you want it to save across attempts)
        cky::totalClicks = 0;   // Reset total
        
        cky::ckyOpenPanel(this->m_uiLayer); 
        cky::ckyBuildHud(this);
        cky::ckyBuildStartPos(this);
        return true;
    }

    // 2. Reset current attempt counters when you die
    void resetLevel() {
        PlayLayer::resetLevel();
        cky::frameClock = 0.0; 
        cky::playbackCursor = 0; 
        cky::clickAccumulator = 0.0f; 
        cky::currentClicks = 0; // Reset current attempt clicks

        if (cky::recording) { cky::macro.clear(); cky::armed = true; }
        if (cky::playing) cky::armed = true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (cky::noclip && object != m_anticheatSpike) return;
        PlayLayer::destroyPlayer(player, object);
    }
};
