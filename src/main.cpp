#include "state.hpp"
#include <Geode/modify/PlayLayer.hpp>

void ckyBuildPanel(PlayLayer* pl);
void ckyBuildHud(PlayLayer* pl);
void ckyBuildStartPos(PlayLayer* pl);

// Define EVERYTHING that state.hpp promised here
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
    
    void ckyNotify(std::string const& msg, bool success) { 
        Notification::create(msg, success ? NotificationIcon::Success : NotificationIcon::Error)->show(); 
    }
    
    void ckyApplyHudScale() {
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_uiLayer) return;
        if (auto hud = pl->m_uiLayer->getChildByID("cky-hud")) hud->setScale(cky::hudScale);
    }
    void ckyApplyAudio() {}
    void ckyApplyFps() {}
    void ckyApplyHitboxes() {}
}

class $modify(CkyPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        cky::frameClock = 0.0;
        cky::playbackCursor = 0;
        cky::armed = false;
        
        cky::ckyOpenPanel(this->m_uiLayer); 
        cky::ckyBuildHud(this);
        cky::ckyBuildStartPos(this);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        cky::frameClock = 0.0;
        cky::playbackCursor = 0;
        cky::clickAccumulator = 0.0f;
        cky::currentClicks = 0;

        if (cky::recording) { cky::macro.clear(); cky::armed = true; }
        if (cky::playing) { cky::armed = true; }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (cky::noclip && object != m_anticheatSpike) return;
        PlayLayer::destroyPlayer(player, object);
    }
};
