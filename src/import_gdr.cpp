// import_gdr.cpp — imports the newest .gdr/.gdr2 replay from <game dir>/macros/import
// Needs the GDReplayFormat library (see CMakeLists). Without it this file is a harmless stub.
#include "state.hpp"

#if __has_include(<gdr/gdr.hpp>)
#include <gdr/gdr.hpp>

namespace {
    struct CkyReplay : gdr::Replay<CkyReplay, gdr::Input<>> {
        CkyReplay() : Replay("ChxrCky", 1) {}
    };

    // works with geode-style (isOk/unwrap) and std::expected-style (bool/*) results
    template <class R> auto resOk(R& r, int) -> decltype(static_cast<bool>(r.isOk())) { return r.isOk(); }
    template <class R> bool resOk(R& r, long) { return static_cast<bool>(r); }
    template <class R> auto resVal(R& r, int) -> decltype(r.unwrap()) { return r.unwrap(); }
    template <class R> auto resVal(R& r, long) -> decltype(*r) { return *r; }
}

bool ckyImportGdr() {
    namespace fs = std::filesystem;
    if (cky::recording) {
        ckyNotify("Stop recording first", false);
        return false;
    }

    std::error_code ec;
    auto dir = cky::macroDir() / "import";
    fs::create_directories(dir, ec);

    fs::path newest;
    fs::file_time_type best{};
    for (auto const& e : fs::directory_iterator(dir, ec)) {
        std::error_code e2;
        if (!e.is_regular_file(e2)) continue;
        std::string ext = e.path().extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext != ".gdr" && ext != ".gdr2") continue;
        auto t = fs::last_write_time(e.path(), e2);
        if (newest.empty() || t > best) { newest = e.path(); best = t; }
    }
    if (newest.empty()) {
        ckyNotify("Put a .gdr file in game/macros/import", false);
        return false;
    }

    std::ifstream f(newest, std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    auto res = CkyReplay::importData(data);
    if (!resOk(res, 0)) {
        ckyNotify("Could not read that file (old GDR1 files are not supported)", false);
        return false;
    }
    auto&& rep = resVal(res, 0);

    std::vector<MacroInput> tmp;
    tmp.reserve(rep.inputs.size());
    for (auto const& in : rep.inputs) {
        MacroInput m;
        m.frame = static_cast<uint32_t>(in.frame);
        m.isPress = in.down;
        m.playerButton = static_cast<int>(in.button) | (in.player2 ? cky::P2_FLAG : 0);
        tmp.push_back(m);
    }
    // GDR stores all P1 inputs then all P2 inputs, so sort by frame
    std::stable_sort(tmp.begin(), tmp.end(), [](MacroInput const& a, MacroInput const& b) {
        return a.frame < b.frame;
    });
    if (tmp.empty()) {
        ckyNotify("That replay has no inputs", false);
        return false;
    }

    cky::macro = std::move(tmp);
    cky::cursor = 0;
    cky::macroTps = rep.framerate > 1.0 ? static_cast<float>(rep.framerate) : 240.f;
    cky::tps = cky::macroTps;
    cky::tpsOn = true;
    ckyNotify(fmt::format("Imported {} inputs @ {} TPS",
        cky::macro.size(), static_cast<int>(cky::macroTps)).c_str());
    return true;
}

#else

bool ckyImportGdr() {
    ckyNotify("GDR library not added (see CMakeLists.txt)", false);
    return false;
}

#endif

// END OF FILE (import_gdr.cpp)
