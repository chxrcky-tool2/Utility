#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace geode::prelude;

struct MacroInput {
    uint32_t frame;
    bool     isPress;
    int      playerButton; // low byte = button id, bit 8 (0x100) = player 2
};

// defined in other files
void ckyHudRefresh(CCNode* hud = nullptr);
CCNode* ckyHudNode();
void ckyBuildHud(PlayLayer* pl);
void ckyApplyHudScale();
void ckyAddIcon(CCNode* host);
void ckyOpenPanel(CCNode* host);
void ckySetStartPosVisible(bool on);
void ckyBuildStartPos(PlayLayer* pl);
void ckyBuildOverlay(PlayLayer* pl);
void ckyOverlayShow(std::string const& text, bool good = true);
void ckyTelemetryPress();
void ckyTelemetryReset();
void ckyClickSound(bool down, bool isPlayer1);
void ckyClickRescan();
std::string ckyClickPackName();
void ckyClickPackStep(int dir);

inline CCPoint ckyDefaultIconPos() {
    auto win = CCDirector::get()->getWinSize();
    return CCPoint(36.f, win.height - 72.f);
}

namespace cky {
    constexpr uint32_t MAGIC1 = 0x31594B43; // "CKY1"
    constexpr uint32_t MAGIC2 = 0x32594B43; // "CKY2"
    constexpr int P2_FLAG = 0x100;
    constexpr uint32_t MAX_ENTRIES = 10000000;
    constexpr int MAX_SLOTS = 20;

    inline std::vector<MacroInput> macro;
    inline uint32_t step = 0;
    inline double   frameClock = 0.0;
    inline size_t   cursor = 0;
    inline bool     recording = false;
    inline bool     playing = false;
    inline bool     injecting = false;
    inline bool     recArmed = false;
    inline bool     playArmed = false;
    inline int      slot = 1;

    inline float    tps = 240.f;
    inline bool     tpsOn = true;
    inline float    macroTps = 240.f;
    inline float    speed = 1.f;
    inline bool     speedOn = true;
    inline bool     frozen = false;
    inline int      pending = 0;
    inline bool     hudOn = true;
    inline uint32_t hudTick = 0;

    inline bool     fpsOn = false;
    inline float    fps = 120.f;
    inline double   origInterval = 0.0;

    inline float    actualTps = 0.f;
    inline uint32_t measSteps = 0;
    inline float    measTime = 0.f;

    inline bool     noclip = false;
    inline bool     safeMode = true;
    inline bool     dirty = false;
    inline bool     hitboxes = false;
    inline bool     startPosOn = true;
    inline bool     overlayOn = true;
    inline bool     clickHudOn = true;
    inline bool     clickbotOn = false;
    inline int      clickPack = 0;
    inline uint32_t clicksTotal = 0;
    inline uint32_t clicksBest = 0;
    inline bool     ckptTelemetry = false;

    inline bool     ncShowAcc = true;
    inline bool     ncShowDeaths = true;
    inline uint32_t ncFrames = 0, ncBad = 0, ncDeaths = 0;
    inline bool     ncBlocked = false, ncPrevBad = false;

    inline float    hudScale = 1.f;
    inline float    iconScale = 1.f;

    inline bool     audioSpeed = true;
    inline bool     lastFrozenAudio = false;

    inline std::vector<float> const tpsList = {
        60.f, 120.f, 144.f, 240.f, 360.f, 480.f, 720.f, 960.f, 1440.f, 1920.f,
        2400.f, 3000.f, 4000.f, 5000.f
    };
    inline std::vector<float> const speedList = {
        0.1f, 0.25f, 0.5f, 0.75f, 1.f, 1.25f, 1.5f, 2.f, 3.f, 4.f
    };
    inline std::vector<float> const fpsList = {
        30.f, 60.f, 90.f, 120.f, 144.f, 240.f, 360.f, 480.f, 1000.f
    };
    inline std::vector<float> const sizeList = { 0.6f, 0.8f, 1.f, 1.2f, 1.5f };
    inline std::vector<float> const highTpsList = { 480.f, 960.f, 1920.f, 3000.f, 5000.f };

    inline float effTps()   { return tpsOn ? tps : 240.f; }
    inline float effSpeed() { return speedOn ? speed : 1.f; }

    inline double clockInc() {
        return static_cast<double>(macroTps) / static_cast<double>(effTps());
    }

    inline bool cheating() {
        return effSpeed() != 1.f || effTps() != 240.f || noclip || playing || frozen;
    }

    inline void resetMeasure() { measSteps = 0; measTime = 0.f; actualTps = 0.f; }

    inline void ncReset() {
        ncFrames = 0; ncBad = 0; ncDeaths = 0;
        ncBlocked = false; ncPrevBad = false;
    }

    inline double ncAccuracy() {
        return ncFrames ? 100.0 * (1.0 - static_cast<double>(ncBad) / static_cast<double>(ncFrames))
                        : 100.0;
    }

    // ---- practice-mode checkpoint tracking (keeps inputs across deaths) ----
    struct Cp { uint32_t step; double clock; size_t macroSize; bool valid; };
    inline std::vector<Cp> ckpts;
    inline bool cpInit = false;

    inline void trackCheckpoints(size_t cnt) {
        if (!recording || !recArmed) { ckpts.clear(); cpInit = false; return; }
        if (!cpInit) {
            ckpts.clear();
            for (size_t i = 0; i < cnt; ++i) ckpts.push_back({ 0, 0.0, 0, false });
            cpInit = true;
            return;
        }
        while (ckpts.size() < cnt) ckpts.push_back({ step, frameClock, macro.size(), true });
        while (ckpts.size() > cnt) ckpts.pop_back();
    }

    inline void rewindTo(Cp const& cp) {
        step = cp.step;
        frameClock = cp.clock;
        cursor = 0;
        if (macro.size() > cp.macroSize) {
            macro.erase(macro.begin() + cp.macroSize, macro.end());
        }
        bool held[0x200] = {};
        for (auto const& e : macro) held[e.playerButton & 0x1FF] = e.isPress;
        uint32_t fr = static_cast<uint32_t>(cp.clock + 0.5);
        for (int i = 0; i < 0x200; ++i) {
            if (held[i]) macro.push_back({ fr, false, i });
        }
    }

    // wipes the whole recording buffer
    inline void clearMacro() {
        macro.clear();
        cursor = 0;
        ckpts.clear();
        cpInit = false;
        playing = false;
    }

    inline float nextVal(std::vector<float> const& v, float cur, int dir) {
        int best = 0;
        for (int i = 0; i < static_cast<int>(v.size()); ++i) {
            if (std::abs(v[i] - cur) < std::abs(v[best] - cur)) best = i;
        }
        int n = std::clamp(best + dir, 0, static_cast<int>(v.size()) - 1);
        return v[n];
    }

    inline float nextHighTps(float cur) {
        for (float t : highTpsList) {
            if (cur < t) return t;
        }
        return 240.f;
    }

    // ---- saved positions ----
    inline CCPoint savedPos(std::string const& key, CCPoint def) {
        float x = Mod::get()->getSavedValue<float>(key + "-x", -99999.f);
        float y = Mod::get()->getSavedValue<float>(key + "-y", -99999.f);
        if (x < -90000.f || y < -90000.f) return def;
        return CCPoint(x, y);
    }

    inline void savePos(std::string const& key, CCPoint p) {
        Mod::get()->setSavedValue<float>(key + "-x", p.x);
        Mod::get()->setSavedValue<float>(key + "-y", p.y);
    }

    // ---- file helpers ----
    template <class T>
    inline void wr(std::ofstream& f, T v) {
        f.write(reinterpret_cast<char const*>(&v), sizeof(T));
    }

    template <class T>
    inline bool rd(std::ifstream& f, T& v) {
        f.read(reinterpret_cast<char*>(&v), sizeof(T));
        return static_cast<bool>(f);
    }

    inline int levelId() {
        if (auto pl = PlayLayer::get()) {
            if (pl->m_level) return pl->m_level->m_levelID.value();
        }
        return 0;
    }

    // <launcher>/game/macros  (falls back to the mod save folder if it can't be created)
    inline std::filesystem::path macroDir() {
        std::error_code ec;
        auto dir = geode::dirs::getGameDir() / "macros";
        std::filesystem::create_directories(dir, ec);
        if (std::filesystem::is_directory(dir, ec)) return dir;
        return Mod::get()->getSaveDir();
    }
    
    inline std::string levelNameSafe() {
        std::string out;
        if (auto pl = PlayLayer::get()) {
            if (pl->m_level) {
                std::string raw = pl->m_level->m_levelName.c_str();
                for (char c : raw) {
                    out += (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
                }
            }
        }
        if (out.empty()) out = "level";
        if (out.size() > 40) out.resize(40);
        return out;
    }

    // e.g. Stereo_Madness_1_slot1.cky
    inline std::filesystem::path slotPath() {
        return macroDir() / fmt::format("{}_{}_slot{}.cky", levelNameSafe(), levelId(), slot);
    }
    
    }

    inline bool slotExists() {
        std::error_code ec;
        return std::filesystem::exists(slotPath(), ec);
    }

    inline bool save() {
        std::ofstream f(slotPath(), std::ios::binary | std::ios::trunc);
        if (!f) return false;
        wr<uint32_t>(f, MAGIC2);
        wr<float>(f, macroTps);
        wr<uint32_t>(f, static_cast<uint32_t>(macro.size()));
        for (auto const& entry : macro) {
            wr<uint32_t>(f, entry.frame);
            wr<uint8_t>(f, entry.isPress ? 1 : 0);
            wr<int32_t>(f, entry.playerButton);
        }
        return f.good();
    }

    inline bool load() {
        auto p = slotPath();
        std::error_code ec;
                if (!std::filesystem::exists(p, ec)) {
            auto oldNamed = macroDir() / fmt::format("macro_{}_slot{}.cky", levelId(), slot);
            if (std::filesystem::exists(oldNamed, ec)) p = oldNamed;
                
        }
        
        // slot 1 also picks up macros saved by older versions
        if (!std::filesystem::exists(p, ec) && slot == 1) {
            auto old = Mod::get()->getSaveDir() / fmt::format("macro_{}.cky", levelId());
            if (std::filesystem::exists(old, ec)) p = old;
            else p = Mod::get()->getSaveDir() / "macro.cky";
        }

        std::ifstream f(p, std::ios::binary);
        if (!f) return false;

        uint32_t magic = 0, count = 0;
        float t = 240.f;
        if (!rd(f, magic)) return false;
        if (magic == MAGIC2) {
            if (!rd(f, t)) return false;
        } else if (magic != MAGIC1) {
            return false;
        }
        if (!rd(f, count)) return false;
        if (count > MAX_ENTRIES || !(t >= 1.f)) return false;

        std::vector<MacroInput> tmp;
        tmp.reserve(count);
        uint32_t lastFrame = 0;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t fr = 0;
            uint8_t press = 0;
            int32_t btn = 0;
            if (!rd(f, fr) || !rd(f, press) || !rd(f, btn)) return false;
            if (fr < lastFrame) return false;
            lastFrame = fr;
            tmp.push_back({ fr, press != 0, btn });
        }
        macro = std::move(tmp);
        macroTps = t;
        cursor = 0;
        return true;
    }
}

// ---------- shared helpers ----------
inline void ckyNotify(char const* text, bool ok = true) {
    Notification::create(text,
        ok ? NotificationIcon::Success : NotificationIcon::Error)->show();
}

inline void ckyApplyAudio() {
    auto engine = FMODAudioEngine::sharedEngine();
    if (!engine) return;
    auto ch = engine->m_backgroundMusicChannel;
    if (!ch) return;
    bool inLevel = PlayLayer::get() != nullptr;
    float pitch = (cky::audioSpeed && inLevel) ? cky::effSpeed() : 1.f;
    ch->setPitch(pitch);
    if (cky::frozen && inLevel) {
        ch->setPaused(true);
    } else if (cky::lastFrozenAudio) {
        ch->setPaused(false);
    }
    cky::lastFrozenAudio = cky::frozen && inLevel;
}

inline void ckyApplyHitboxes() {
    auto pl = PlayLayer::get();
    if (!pl) return;
    if (cky::hitboxes) {
        pl->m_isDebugDrawEnabled = true;
        if (pl->m_debugDrawNode) pl->m_debugDrawNode->setVisible(true);
    } else {
        bool gameWants = GameManager::get()->getGameVariable("0166");
        pl->m_isDebugDrawEnabled = gameWants;
        if (!gameWants && pl->m_debugDrawNode) pl->m_debugDrawNode->clear();
    }
}

inline void ckyApplyFps() {
    auto dir = CCDirector::get();
    if (cky::origInterval <= 0.0) {
        if (!cky::fpsOn) return;
        cky::origInterval = dir->getAnimationInterval();
    }
    double iv = cky::fpsOn ? 1.0 / static_cast<double>(cky::fps) : cky::origInterval;
    CCApplication::sharedApplication()->setAnimationInterval(iv);
}

// END OF FILE (state.hpp)
