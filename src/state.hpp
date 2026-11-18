#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <array>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <zstd.h>
#include <string>
#include <vector>
#include "compat_v6.hpp"

using namespace geode::prelude;

#include "player_state.hpp"
#include "fixstate.hpp"
#include "subframe.hpp"
#include "themes.hpp"
#include "framewindow.hpp"

struct FrameFix {
    uint32_t frame;
    float x1, y1, r1;
    float x2, y2, r2;
    double yv1 = 0.0, yv2 = 0.0;
    bool   hasVel = true;
    FixExtra e1, e2;
    bool   hasExtra = false;
};

struct LoggedInput {
    uint32_t step;
    bool     press;
    int      btn;
};

struct MacroInput {
    uint32_t frame;
    bool     isPress;
    int      playerButton;
};

void ckyHudRefresh(CCNode* hud = nullptr);
CCNode* ckyHudNode();
void ckyBuildHud(PlayLayer* pl);
void ckyApplyHudScale();
void ckyAddIcon(CCNode* host);
void ckyOpenPanel(CCNode* host);
void ckyToggleCocosPanel(CCNode* host);
void ckySetStartPosVisible(bool on);
void ckyBuildStartPos(PlayLayer* pl);
void ckyBuildOverlay(PlayLayer* pl);
void ckyBuildLabels(PlayLayer* pl);
void ckyRunReset();
void ckyRunBegin();
void ckyRunDeath();
void ckyRunComplete();
void ckyOverlayShow(std::string const& text, bool good = true);
void ckyTelemetryPress();
void ckyTelemetryReset();
void ckyClickSound(bool down, bool isPlayer1);
void ckyClickRescan();
std::string ckyClickPackName();
void ckyClickPackStep(int dir);
bool ckyImportGdr();
bool ckyTrajHandleDeath(PlayerObject* player, GameObject* object = nullptr);
float ckyTrajStepDelta();
void  ckyTrajResetCancel();
bool  ckyTrajCancelled();
bool  ckyTrajKiller(CCPoint* topCenter);
void ckyTrajClear();
bool ckyUiIsImGui();
void ckyImGuiToggle();
void ckyBackStepCapture();
void ckyBackStepClear();
bool ckyBackStep();
size_t ckyBackStepCount();
void ckyBackStepFinish();
void ckyInputLog(bool down, int playerButton);
void ckyInputLogReset();
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
    constexpr uint32_t MAGIC1 = 0x31594B43;
    constexpr uint32_t MAGIC2 = 0x32594B43;
    constexpr uint32_t MAGIC3 = 0x33594B43;
    constexpr uint32_t MAGIC4 = 0x34594B43;
    constexpr uint32_t MAGIC5 = 0x35594B43;
    constexpr uint32_t MAGIC6 = 0x36594B43;
    constexpr uint32_t MAGICZ = 0x355A4B43;
    constexpr int ZSTD_LEVEL = 3;
    inline bool compressSaves = true;
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
    inline std::string macroName;
    constexpr size_t MACRO_NAME_MAX = 48;

    inline std::vector<FrameFix> fixes;
    inline size_t   fixCursor = 0;
    inline bool     frameFixes = true;
    inline int      fixInterval = 1;
    inline bool     fixVelocity = true;
    inline bool     trajectory = false;
    inline bool     trajCreating = false;
    inline int      trajLen = 200;

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

    inline bool     deltaLock = true;
    inline float    deltaLockTps = 30.f;

    inline float    actualTps = 0.f;
    inline uint32_t measSteps = 0;
    inline float    measTime = 0.f;

    inline bool     noclip = false;
    inline bool     safeMode = true;
    inline bool     dirty = false;
    inline bool     hitboxes = false;
    inline bool     hitboxTrail = false;
    inline bool     practiceFixes = true;
    inline bool     anchoring = false;
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
    inline int      clickVol = 100;
    inline float    tpsStep = 10.f;
    inline bool     imguiOpen = false;
    inline bool     stepperBtns = true;
    inline bool     speedBtns = true;
    inline bool     hudStepShow = true;
    inline bool     hudSpeedShow = true;
    inline bool     backStepOn = true;
    inline bool     backAlways = false;
    inline int      backDepth = 240;

    inline bool     resimming = false;
    inline int      resimLeft = 0;
    inline std::vector<int> resimHeld;
    inline std::deque<LoggedInput> inputLog;
    inline bool     held[0x200] = {};

    inline bool     verifyOn = true;
    inline double   verifyTolPos = 0.001;
    inline double   verifyTolVel = 0.001;
    inline uint32_t verifyChecks = 0;
    inline uint32_t verifyMisses = 0;
    inline int64_t  firstDivFrame = -1;
    inline void verifyReset() { verifyChecks = 0; verifyMisses = 0; firstDivFrame = -1; }

    inline int      frameOffset = 0;
    inline bool     holdFix[0x200] = {};
    inline bool     holdFixPending = false;

    inline bool     ncShowAcc = true;
    inline bool     ncShowDeaths = true;
    inline uint32_t ncFrames = 0, ncBad = 0, ncDeaths = 0;
    inline bool     ncBlocked = false, ncPrevBad = false;

    inline float    hudScale = 1.f;
    inline float    iconScale = 1.f;
    inline float    hudOpacity = 0.75f;

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

    inline void hudOpacitySave() {
        Mod::get()->setSavedValue<float>("hud-opacity", hudOpacity);
    }
    inline void hudOpacityLoad() {
        hudOpacity = std::clamp(Mod::get()->getSavedValue<float>("hud-opacity", hudOpacity), 0.1f, 1.f);
    }

    inline float effTps()   { return tpsOn ? tps : 240.f; }
    inline float effSpeed() { return (speedOn && speedBtns) ? speed : 1.f; }
    inline bool  deltaLockActive() { return deltaLock && effTps() > 240.f && !frozen; }

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

    inline void rewindTo(Cp const& cp) {
        step = cp.step;
        frameClock = cp.clock;
        cursor = 0;
        {
            uint32_t cf = static_cast<uint32_t>(cp.clock + 0.5);
            while (!fixes.empty() && fixes.back().frame >= cf) fixes.pop_back();
            fixCursor = 0;
            while (!sfBlobs.empty() && sfBlobs.back().frame >= cf) sfBlobs.pop_back();
            sfCursor = 0;
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
        sfCursor = 0;
        while (sfCursor < sfBlobs.size() && static_cast<double>(sfBlobs[sfCursor].frame) < cp.clock) ++sfCursor;
    }

    inline void clearMacro() {
        macro.clear();
        fixes.clear();
        fixCursor = 0;
        sfBlobs.clear();
        sfCursor = 0;
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

    template <class T, class S>
    inline void wr(S& f, T v) {
        f.write(reinterpret_cast<char const*>(&v), sizeof(T));
    }

    template <class T, class S>
    inline bool rd(S& f, T& v) {
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

    inline std::filesystem::path macroDir() {
        std::error_code ec;
        auto dir = stdPath(geode::dirs::getGameDir()) / "macros";
        std::filesystem::create_directories(dir, ec);
        if (std::filesystem::is_directory(dir, ec)) return dir;
        return stdPath(Mod::get()->getSaveDir());
    }

    inline std::filesystem::path themeMacroDir(int t) {
        auto const& d = kThemeDefs[std::clamp(t, 0, ThemeCount - 1)];
        return stdPath(Mod::get()->getConfigDir()) / "Theme macros" / d.name;
    }

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
        if (ok) log::info("[cky] macro folders ready: {}", (themeMacroDir(0).parent_path()).string());
        else    log::error("[cky] could not create macro folders under {}: {}", (themeMacroDir(0).parent_path()).string(), ec.message());
    }

    inline bool macroCharOk(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == ' ' || c == '_';
    }

    inline std::string filterMacroName(std::string const& in) {
        std::string out;
        for (char c : in) {
            if (macroCharOk(c) && out.size() < MACRO_NAME_MAX) out += c;
        }
        return out;
    }

    inline std::string macroStem() {
        std::string s = filterMacroName(macroName);
        size_t b = s.find_first_not_of(' ');
        if (b == std::string::npos) return {};
        size_t e = s.find_last_not_of(' ');
        return s.substr(b, e - b + 1);
    }

    inline bool macroNameOk() {
        return !macroStem().empty();
    }

    inline void setMacroName(std::string const& v) {
        macroName = filterMacroName(v);
        Mod::get()->setSavedValue<std::string>("s-macro-name", macroName);
    }

    inline std::string macroExt(int t) {
        return kThemeDefs[std::clamp(t, 0, ThemeCount - 1)].ext;
    }

    inline std::string macroFileName() {
        return macroStem() + macroExt(theme);
    }

    inline std::filesystem::path slotFile(int t) {
        return themeMacroDir(t) / (macroStem() + macroExt(t));
    }

    inline std::filesystem::path slotPath() {
        if (!macroNameOk()) return {};
        std::error_code ec;
        std::filesystem::create_directories(themeMacroDir(theme), ec);
        return slotFile(theme);
    }

    inline std::filesystem::path slotFind() {
        std::filesystem::path best;
        if (!macroNameOk()) return best;
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
        for (int i = 0; i < ThemeCount; ++i) consider(macroDir() / (macroStem() + macroExt(i)));
        return best;
    }

    inline bool slotExists() {
        return !slotFind().empty();
    }

    inline bool save() {
        std::ostringstream f(std::ios::binary);
        wr<uint32_t>(f, MAGIC6);
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
            for (FixExtra const* e : { &x.e1, &x.e2 }) {
                wr<uint8_t>(f, e->flags);
                wr<double>(f, e->dashX); wr<double>(f, e->dashY);
                wr<double>(f, e->dashAngle); wr<double>(f, e->dashStart);
            }
        }
        wr<uint32_t>(f, ckySfLayoutHash());
        wr<uint32_t>(f, static_cast<uint32_t>(sfBlobs.size()));
        for (auto const& b : sfBlobs) {
            wr<uint32_t>(f, b.frame);
            wr<uint8_t>(f, b.dual);
            wr<uint16_t>(f, b.n[0]);
            wr<uint16_t>(f, b.n[1]);
            for (int i = 0; i < 2; ++i) {
                wr<float>(f, b.px[i]); wr<float>(f, b.py[i]); wr<float>(f, b.rot[i]);
            }
            f.write(reinterpret_cast<char const*>(b.bytes.data()), static_cast<std::streamsize>(b.bytes.size()));
        }
        if (!f.good()) return false;
        std::string raw = std::move(f).str();

        auto path = slotPath();
        if (path.empty()) return false;
        auto tmp = path; tmp += ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) return false;
            bool zipped = false;
            if (compressSaves && raw.size() > 64) {
                std::string z(ZSTD_compressBound(raw.size()), '\0');
                size_t n = ZSTD_compress(z.data(), z.size(), raw.data(), raw.size(), ZSTD_LEVEL);
                if (!ZSTD_isError(n) && n + 8 < raw.size()) {
                    wr<uint32_t>(out, MAGICZ);
                    wr<uint32_t>(out, static_cast<uint32_t>(raw.size()));
                    out.write(z.data(), static_cast<std::streamsize>(n));
                    zipped = true;
                    log::info("[cky] saved macro: {} -> {} bytes (zstd {})", raw.size(), n + 8, ZSTD_LEVEL);
                }
            }
            if (!zipped) out.write(raw.data(), static_cast<std::streamsize>(raw.size()));
            if (!out.good()) return false;
        }
        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            std::filesystem::remove(path, ec);
            std::filesystem::rename(tmp, path, ec);
        }
        return !ec;
    }

    inline bool load() {
        auto p = slotFind();
        if (p.empty()) return false;

        std::string bytes;
        {
            std::ifstream in(p, std::ios::binary);
            if (!in) return false;
            std::ostringstream ss;
            ss << in.rdbuf();
            bytes = std::move(ss).str();
        }
        if (bytes.size() >= 8) {
            uint32_t m0 = 0, rawSize = 0;
            std::memcpy(&m0, bytes.data(), 4);
            if (m0 == MAGICZ) {
                std::memcpy(&rawSize, bytes.data() + 4, 4);
                if (rawSize == 0 || rawSize > 1024u * 1024u * 1024u) return false;
                std::string raw(rawSize, '\0');
                size_t n = ZSTD_decompress(raw.data(), raw.size(), bytes.data() + 8, bytes.size() - 8);
                if (ZSTD_isError(n) || n != rawSize) {
                    log::error("[cky] macro decompress failed: {}", ZSTD_isError(n) ? ZSTD_getErrorName(n) : "size mismatch");
                    return false;
                }
                bytes = std::move(raw);
            }
        }
        std::istringstream f(bytes, std::ios::binary);

        uint32_t magic = 0, count = 0;
        float t = 240.f;
        if (!rd(f, magic)) return false;
        if (magic == MAGIC2 || magic == MAGIC3 || magic == MAGIC4 || magic == MAGIC5 || magic == MAGIC6) {
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
        if (magic == MAGIC3 || magic == MAGIC4 || magic == MAGIC5 || magic == MAGIC6) {
            uint32_t fc = 0;
            if (!rd(f, fc) || fc > MAX_ENTRIES) return false;
            fx.reserve(fc);
            uint32_t lastFix = 0;
            for (uint32_t i = 0; i < fc; ++i) {
                FrameFix x{};
                if (!rd(f, x.frame) || !rd(f, x.x1) || !rd(f, x.y1) || !rd(f, x.r1)
                    || !rd(f, x.x2) || !rd(f, x.y2) || !rd(f, x.r2)) return false;
                bool v4 = (magic == MAGIC4 || magic == MAGIC5 || magic == MAGIC6);
                if (v4 && (!rd(f, x.yv1) || !rd(f, x.yv2))) return false;
                x.hasVel = v4;
                if (magic == MAGIC5 || magic == MAGIC6) {
                    for (FixExtra* e : { &x.e1, &x.e2 }) {
                        if (!rd(f, e->flags) || !rd(f, e->dashX) || !rd(f, e->dashY)
                            || !rd(f, e->dashAngle) || !rd(f, e->dashStart)) return false;
                    }
                    x.hasExtra = true;
                }
                if (x.frame < lastFix) return false;
                lastFix = x.frame;
                fx.push_back(x);
            }
        }
        std::vector<CkySfBlob> sf;
        if (magic == MAGIC6) {
            uint32_t hash = 0, sc = 0;
            if (!rd(f, hash) || !rd(f, sc) || sc > MAX_ENTRIES) return false;
            bool useIt = (hash == ckySfLayoutHash());
            uint32_t lastSf = 0;
            for (uint32_t i = 0; i < sc; ++i) {
                CkySfBlob b;
                uint8_t dual = 0;
                if (!rd(f, b.frame) || !rd(f, dual) || !rd(f, b.n[0]) || !rd(f, b.n[1])) return false;
                if (b.n[0] > 8192 || b.n[1] > 8192) return false;
                for (int k = 0; k < 2; ++k) {
                    if (!rd(f, b.px[k]) || !rd(f, b.py[k]) || !rd(f, b.rot[k])) return false;
                }
                b.dual = dual;
                b.bytes.resize(static_cast<size_t>(b.n[0]) + static_cast<size_t>(b.n[1]));
                f.read(reinterpret_cast<char*>(b.bytes.data()), static_cast<std::streamsize>(b.bytes.size()));
                if (!f) return false;
                if (b.frame < lastSf) return false;
                lastSf = b.frame;
                if (useIt) sf.push_back(std::move(b));
            }
            if (!useIt && sc > 0) {
                log::warn("[cky] macro has {} sub-frame snapshots from a different player layout, ignored", sc);
            }
        }
        macro = std::move(tmp);
        fixes = std::move(fx);
        sfBlobs = std::move(sf);
        sfCursor = 0;
        fixCursor = 0;
        macroTps = t;
        cursor = 0;
        return true;
    }
}

namespace cky {
    inline bool lblFps  = false;
    inline bool lblRun  = false;
    inline bool lblTime = false;
    inline bool lblBest = false;

    inline void labelsSave() {
        auto m = Mod::get();
        m->setSavedValue<bool>("lbl-cps", clickHudOn);
        m->setSavedValue<bool>("lbl-fps", lblFps);
        m->setSavedValue<bool>("lbl-nc-acc", ncShowAcc);
        m->setSavedValue<bool>("lbl-nc-deaths", ncShowDeaths);
        m->setSavedValue<bool>("lbl-run", lblRun);
        m->setSavedValue<bool>("lbl-time", lblTime);
        m->setSavedValue<bool>("lbl-best", lblBest);
    }

    inline void labelsLoad() {
        auto m = Mod::get();
        clickHudOn   = m->getSavedValue<bool>("lbl-cps", clickHudOn);
        lblFps       = m->getSavedValue<bool>("lbl-fps", lblFps);
        ncShowAcc    = m->getSavedValue<bool>("lbl-nc-acc", ncShowAcc);
        ncShowDeaths = m->getSavedValue<bool>("lbl-nc-deaths", ncShowDeaths);
        lblRun       = m->getSavedValue<bool>("lbl-run", lblRun);
        lblTime      = m->getSavedValue<bool>("lbl-time", lblTime);
        lblBest      = m->getSavedValue<bool>("lbl-best", lblBest);
    }
}

template <class M>
inline void ckyFlushMod(M* m) {
    if constexpr (requires { m->saveData(); }) {
        (void)m->saveData();
    }
}

namespace cky {
    struct BoolSetting  { char const* key; bool* ref; };
    struct FloatSetting { char const* key; float* ref; float lo; float hi; };
    struct IntSetting   { char const* key; int* ref; int lo; int hi; };

    inline std::vector<BoolSetting> const& boolSettings() {
        static std::vector<BoolSetting> const v = {
            { "s-noclip", &noclip },
            { "s-safe", &safeMode },
            { "s-hitboxes", &hitboxes },
            { "s-trail", &hitboxTrail },
            { "s-pfix", &practiceFixes },
            { "s-ffix", &frameFixes },
            { "s-fixvel", &fixVelocity },
            { "s-verify", &verifyOn },
            { "s-startpos", &startPosOn },
            { "s-overlay", &overlayOn },
            { "s-clickbot", &clickbotOn },
            { "s-telem", &ckptTelemetry },
            { "s-traj", &trajectory },
            { "s-tps-on", &tpsOn },
            { "s-speed-on", &speedOn },
            { "s-audio", &audioSpeed },
            { "s-hud", &hudOn },
            { "s-step-btns", &stepperBtns },
            { "s-speed-btns", &speedBtns },
            { "s-hud-step", &hudStepShow },
            { "s-hud-speed", &hudSpeedShow },
            { "s-backstep", &backStepOn },
            { "s-back-always", &backAlways },
            { "s-delta-lock", &deltaLock },
            { "lbl-cps", &clickHudOn },
            { "lbl-fps", &lblFps },
            { "lbl-nc-acc", &ncShowAcc },
            { "lbl-nc-deaths", &ncShowDeaths },
            { "lbl-run", &lblRun },
            { "lbl-time", &lblTime },
            { "lbl-best", &lblBest },
        };
        return v;
    }

    inline std::vector<FloatSetting> const& floatSettings() {
        static std::vector<FloatSetting> const v = {
            { "s-tps", &tps, TPS_MIN, TPS_MAX },
            { "s-speed", &speed, 0.01f, 4.f },
            { "s-tps-step", &tpsStep, 1.f, 500.f },
            { "s-hud-scale", &hudScale, 0.5f, 2.f },
            { "s-icon-scale", &iconScale, 0.5f, 2.f },
            { "s-delta-tps", &deltaLockTps, 20.f, 120.f },
        };
        return v;
    }

    inline std::vector<IntSetting> const& intSettings() {
        static std::vector<IntSetting> const v = {
            { "s-fix-interval", &fixInterval, 1, 240 },
            { "s-back-depth", &backDepth, 30, 600 },
            { "s-sf-depth", &sfDepth, 30, 960 },
            { "s-sf-interval", &sfInterval, 1, 960 },
            { "s-traj-len", &trajLen, 60, 480 },
            { "s-frame-offset", &frameOffset, -10, 10 },
            { "s-slot", &slot, 1, MAX_SLOTS },
        };
        return v;
    }

    inline std::vector<double> settingsLast;

    inline std::vector<double> settingsValues() {
        std::vector<double> v;
        for (auto const& s : boolSettings()) v.push_back(*s.ref ? 1.0 : 0.0);
        for (auto const& s : floatSettings()) v.push_back(static_cast<double>(*s.ref));
        for (auto const& s : intSettings()) v.push_back(static_cast<double>(*s.ref));
        return v;
    }

    inline void settingsSave() {
        auto m = Mod::get();
        for (auto const& s : boolSettings()) m->setSavedValue<bool>(s.key, *s.ref);
        for (auto const& s : floatSettings()) m->setSavedValue<float>(s.key, *s.ref);
        for (auto const& s : intSettings()) m->setSavedValue<int>(s.key, *s.ref);
        settingsLast = settingsValues();
        ckyFlushMod(m);
    }

    inline void settingsSaveIfChanged() {
        if (settingsLast == settingsValues()) return;
        settingsSave();
    }

    inline void settingsLoad() {
        auto m = Mod::get();
        for (auto const& s : boolSettings()) *s.ref = m->getSavedValue<bool>(s.key, *s.ref);
        for (auto const& s : floatSettings()) {
            *s.ref = std::clamp(m->getSavedValue<float>(s.key, *s.ref), s.lo, s.hi);
        }
        for (auto const& s : intSettings()) {
            *s.ref = std::clamp(m->getSavedValue<int>(s.key, *s.ref), s.lo, s.hi);
        }
        macroName = filterMacroName(m->getSavedValue<std::string>("s-macro-name", macroName));
        settingsLast = settingsValues();
    }
}

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

