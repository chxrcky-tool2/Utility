#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace geode::prelude;

#include "player_state.hpp"
#include "themes.hpp"

// per-step player snapshot used by "Frame fixes" (position + rotation of both players)
struct FrameFix {
    uint32_t frame;
    float x1, y1, r1;
    float x2, y2, r2;
    double yv1 = 0.0, yv2 = 0.0; // vertical velocity of each player (CKY4)
    bool   hasVel = true;        // false for fixes loaded from old CKY3 files
};

// one input that reached the game, tagged with the physics step it happened on (back step re-sim)
struct LoggedInput {
    uint32_t step;
    bool     press;
    int      btn; // low byte = button id, bit 8 (0x100) = player 2
};

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
bool ckyImportGdr();
bool ckyTrajHandleDeath(PlayerObject* player); // true = it was a trajectory fake player
void ckyTrajClear();
bool ckyUiIsImGui();
void ckyImGuiToggle();
// frame stepper back step (backstep.cpp)
void ckyBackStepCapture();
void ckyBackStepClear();
bool ckyBackStep();
size_t ckyBackStepCount();
void ckyBackStepFinish();
void ckyInputLog(bool down, int playerButton);
void ckyInputLogReset();
// hitbox trail (hitboxtrail.cpp)
void ckyTrailCapture(GJBaseGameLayer* layer);
void ckyTrailRewind(uint32_t step);
void ckyTrailReset();
void ckyTrailApply();
void ckyTrailSave();
void ckyTrailLoad();

inline CCPoint ckyDefaultIconPos() {
    auto win = CCDirector::get()->getWinSize();
    return CCPoint(36.f, win.height - 72.f);
}

namespace cky {
    constexpr uint32_t MAGIC1 = 0x31594B43; // "CKY1"
    constexpr uint32_t MAGIC2 = 0x32594B43; // "CKY2"
    constexpr uint32_t MAGIC3 = 0x33594B43; // "CKY3" (CKY2 + frame fixes: position + rotation)
    constexpr uint32_t MAGIC4 = 0x34594B43; // "CKY4" (CKY3 + vertical velocity in frame fixes)
    constexpr int P2_FLAG = 0x100;
    constexpr uint32_t MAX_ENTRIES = 10000000;
    constexpr int MAX_SLOTS = 20;
    constexpr float TPS_MIN = 1.f, TPS_MAX = 6000.f;

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

    // frame fixes + trajectory
    inline std::vector<FrameFix> fixes;
    inline size_t   fixCursor = 0;
    inline bool     frameFixes = true;
    inline int      fixInterval = 1;      // record a frame fix every N steps (in 240-TPS steps)
    inline bool     fixVelocity = true;   // also restore vertical velocity on playback
    inline bool     trajectory = false;
    inline bool     trajCreating = false;
    inline int      trajLen = 200;        // simulated steps (60-480)

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
    // hitbox trail: RGBA (0-1) of the outer and inner hitbox, length in physics steps
    inline bool     hitboxTrail = false;
    // practice fixes (xdBot idea): re-apply the full player state after a checkpoint / anchor load
    inline bool     practiceFixes = true;
    inline bool     anchoring = false;   // true while the back stepper takes its own snapshot
    inline int      trailLen = 120;
    inline float    trailOuter[4] = { 1.f, 0.25f, 0.25f, 1.f };
    inline float    trailInner[4] = { 0.30f, 0.55f, 1.f, 1.f };
    inline bool     startPosOn = true;
    inline bool     overlayOn = true;
    inline bool     clickHudOn = true;
    inline bool     clickbotOn = false;
    inline int      clickPack = 0;
    inline uint32_t clicksTotal = 0;
    inline uint32_t clicksBest = 0;
    inline bool     ckptTelemetry = false;
    inline int      theme = 0;
    // ThemeCount + every theme's colours live in themes.hpp
    inline int      clickVol = 100;   // clickbot volume, percent (0-200)
    inline float    tpsStep = 10.f;   // how much the TPS +/- buttons change
    inline bool     imguiOpen = false;
    inline bool     stepperBtns = true;  // frame stepper buttons exist in the level HUD
    inline bool     speedBtns = true;    // speedhack buttons exist in the level HUD (off = 1.00x)
    inline bool     hudStepShow = true;  // show the frame stepper box (top-left)
    inline bool     hudSpeedShow = true; // show the speedhack box (top-right)
    inline bool     backStepOn = true;   // keep rewind snapshots while the stepper is paused
    // Rolling rewind buffer while playing. OFF by default: it calls createCheckpoint() (a full
    // copy of the level state) on EVERY physics step, which is what capped real TPS.
    inline bool     backAlways = false;
    inline int      backDepth = 240;     // how many steps you can go back (30-600)

    // back step re-simulation state
    inline bool     resimming = false;
    inline int      resimLeft = 0;
    inline std::vector<int> resimHeld;
    inline std::deque<LoggedInput> inputLog;
    inline bool     held[0x200] = {};    // buttons currently held (as seen by the game)

    // exact-frame desync verification
    inline bool     verifyOn = true;
    inline double   verifyTolPos = 0.001;
    inline double   verifyTolVel = 0.001;
    inline uint32_t verifyChecks = 0;
    inline uint32_t verifyMisses = 0;
    inline int64_t  firstDivFrame = -1;
    inline void verifyReset() { verifyChecks = 0; verifyMisses = 0; firstDivFrame = -1; }

    // input fixes
    inline int      frameOffset = 0;
    inline bool     holdFix[0x200] = {};
    inline bool     holdFixPending = false;

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
    inline std::vector<float> const fixIntervalList = {
        1.f, 2.f, 4.f, 8.f, 15.f, 30.f, 60.f, 120.f, 240.f
    };
    inline std::vector<float> const sizeList = { 0.6f, 0.8f, 1.f, 1.2f, 1.5f };
    inline std::vector<float> const tpsStepList = { 1.f, 5.f, 10.f, 50.f, 100.f, 500.f };
    inline std::vector<float> const highTpsList = { 480.f, 960.f, 1920.f, 3000.f, 5000.f };

    inline float effTps()   { return tpsOn ? tps : 240.f; }
    inline float effSpeed() { return (speedOn && speedBtns) ? speed : 1.f; }

    inline double clockInc() {
        return static_cast<double>(macroTps) / static_cast<double>(effTps());
    }

    inline bool cheating() {
        return effSpeed() != 1.f || effTps() != 240.f || noclip || playing || frozen || trajectory;
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
        if (!(recording && recArmed) && !(playing && playArmed)) {
            ckpts.clear();
            cpInit = false;
            return;
        }
        if (!cpInit) {
            ckpts.clear();
            for (size_t i = 0; i < cnt; ++i) ckpts.push_back({ 0, 0.0, 0, false });
            cpInit = true;
            return;
        }
        while (ckpts.size() < cnt) ckpts.push_back({ step, frameClock, macro.size(), true });
        while (ckpts.size() > cnt) ckpts.pop_back();
    }

    // recording: cut the macro back to the checkpoint
    inline void rewindTo(Cp const& cp) {
        step = cp.step;
        frameClock = cp.clock;
        cursor = 0;
        {
            uint32_t cf = static_cast<uint32_t>(cp.clock + 0.5);
            while (!fixes.empty() && fixes.back().frame >= cf) fixes.pop_back();
            fixCursor = 0;
        }
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

    // playback: jump the clock and cursor back to a checkpoint; buttons that should be
    // held at that moment get re-pressed on the next step
    inline void rewindPlaybackTo(Cp const& cp) {
        step = cp.step;
        frameClock = cp.clock;
        cursor = 0;
        for (auto& h : holdFix) h = false;
        while (cursor < macro.size() && static_cast<double>(macro[cursor].frame) < cp.clock) {
            holdFix[macro[cursor].playerButton & 0x1FF] = macro[cursor].isPress;
            ++cursor;
        }
        holdFixPending = true;
        fixCursor = 0;
        while (fixCursor < fixes.size() && static_cast<double>(fixes[fixCursor].frame) < cp.clock) ++fixCursor;
    }

    // wipes the whole recording buffer
    inline void clearMacro() {
        macro.clear();
        fixes.clear();
        fixCursor = 0;
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

    // <launcher>/game/macros  (falls back to the mod save folder if it can't be created)
    inline std::filesystem::path macroDir() {
        std::error_code ec;
        auto dir = geode::dirs::getGameDir() / "macros";
        std::filesystem::create_directories(dir, ec);
        if (std::filesystem::is_directory(dir, ec)) return dir;
        return Mod::get()->getSaveDir();
    }

    // Saved macros live in the mod's CONFIG folder (Android: com.geode.launcher/.../game/geode/config/chxrcky.cky):
    //   chxrcky.cky / Theme macros / <Theme name> / <level>_<id>_slot<N><ext>
    // e.g. Theme macros/Cat/Stereo_Madness_1_slot1.meow, Theme macros/Vintage/...slot1.vinyl,
    // Theme macros/Molten rocks/...slot1.lava. Inside is the same CKY format whatever the extension.
    // Old ".cky" macros in <game>/macros still load.
    inline std::filesystem::path themeMacroDir(int t) {
        auto const& d = kThemeDefs[std::clamp(t, 0, ThemeCount - 1)];
        return Mod::get()->getConfigDir() / "Theme macros" / d.name;
    }

    // creates "Theme macros" and one folder per theme (when the mod loads and when the menu opens,
    // so they are there to browse). Cheap: does nothing after the first successful run.
    inline void makeMacroFolders() {
        static bool done = false;
        if (done) return;
        std::error_code ec;
        bool ok = true;
        for (int i = 0; i < ThemeCount; ++i) {
            std::filesystem::create_directories(themeMacroDir(i), ec);
            if (ec) ok = false;
        }
        done = ok;
    }

    // the file this level + slot has under theme t
    inline std::filesystem::path slotFile(int t) {
        auto const& d = kThemeDefs[std::clamp(t, 0, ThemeCount - 1)];
        return themeMacroDir(t) / fmt::format("{}_{}_slot{}{}", levelNameSafe(), levelId(), slot, d.ext);
    }

    // where a new macro is written: the current theme's folder + extension
    inline std::filesystem::path slotPath() {
        std::error_code ec;
        std::filesystem::create_directories(themeMacroDir(theme), ec);
        return slotFile(theme);
    }

    // the macro to read: the most recently saved file among every theme's folder
    // (current theme first, so it wins a tie) and the legacy <game>/macros/*.cky.
    // Empty path if there is none.
    inline std::filesystem::path slotFind() {
        std::filesystem::path best;
        std::filesystem::file_time_type bestTime{};
        auto consider = [&](std::filesystem::path p) {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(p, ec) || ec) return;
            auto t = std::filesystem::last_write_time(p, ec);
            if (ec) return;
            if (best.empty() || t > bestTime) { best = std::move(p); bestTime = t; }
        };
        consider(slotFile(theme));
        for (int i = 0; i < ThemeCount; ++i) consider(slotFile(i));
        consider(macroDir() / fmt::format("{}_{}_slot{}.cky", levelNameSafe(), levelId(), slot));
        return best;
    }

    inline bool slotExists() {
        return !slotFind().empty();
    }

    inline bool save() {
        std::ofstream f(slotPath(), std::ios::binary | std::ios::trunc);
        if (!f) return false;
        wr<uint32_t>(f, MAGIC4);
        wr<float>(f, macroTps);
        wr<uint32_t>(f, static_cast<uint32_t>(macro.size()));
        for (auto const& entry : macro) {
            wr<uint32_t>(f, entry.frame);
            wr<uint8_t>(f, entry.isPress ? 1 : 0);
            wr<int32_t>(f, entry.playerButton);
        }
        wr<uint32_t>(f, static_cast<uint32_t>(fixes.size()));
        for (auto const& x : fixes) {
            wr<uint32_t>(f, x.frame);
            wr<float>(f, x.x1); wr<float>(f, x.y1); wr<float>(f, x.r1);
            wr<float>(f, x.x2); wr<float>(f, x.y2); wr<float>(f, x.r2);
            wr<double>(f, x.yv1); wr<double>(f, x.yv2);
        }
        return f.good();
    }

    inline bool load() {
        auto p = slotFind();
        std::error_code ec;
        if (p.empty()) {
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
        if (magic == MAGIC2 || magic == MAGIC3 || magic == MAGIC4) {
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
        std::vector<FrameFix> fx;
        if (magic == MAGIC3 || magic == MAGIC4) {
            uint32_t fc = 0;
            if (!rd(f, fc) || fc > MAX_ENTRIES) return false;
            fx.reserve(fc);
            uint32_t lastFix = 0;
            for (uint32_t i = 0; i < fc; ++i) {
                FrameFix x{};
                if (!rd(f, x.frame) || !rd(f, x.x1) || !rd(f, x.y1) || !rd(f, x.r1)
                    || !rd(f, x.x2) || !rd(f, x.y2) || !rd(f, x.r2)) return false;
                // CKY3 files have no velocity: yv stays 0 and is skipped on playback
                if (magic == MAGIC4 && (!rd(f, x.yv1) || !rd(f, x.yv2))) return false;
                x.hasVel = (magic == MAGIC4);
                if (x.frame < lastFix) return false;
                lastFix = x.frame;
                fx.push_back(x);
            }
        }
        macro = std::move(tmp);
        fixes = std::move(fx);
        fixCursor = 0;
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
