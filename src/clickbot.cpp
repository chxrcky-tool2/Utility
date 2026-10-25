// clickbot.cpp — clickpack loader + click sounds
// packs live in <game dir>/clicks/<PackName>/player1/{clicks,releases,softclicks,...}
#include "state.hpp"
#include <random>

namespace {
    // gaps between presses (seconds) that choose the click type: tweak to taste
    constexpr double HARD_GAP = 2.0;
    constexpr double NORMAL_GAP = 0.3;
    constexpr double SOFT_GAP = 0.08;

    enum Kind { HARD = 0, NORMAL = 1, SOFT = 2, MICRO = 3 };
    using Files = std::vector<std::string>;

    struct Pack {
        Files clicks, releases, soft, softRel, hard, hardRel, micro, microRel;
    };

    Pack g_pack[2];
    std::vector<std::string> g_names; // "" = the clicks folder itself
    bool g_scanned = false;
    std::mt19937 g_rng{ std::random_device{}() };
    double g_lastPress[2] = { -100.0, -100.0 };
    int g_lastKind[2] = { NORMAL, NORMAL };

    std::string lower(std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::filesystem::path root() { return geode::dirs::getGameDir() / "clicks"; }

    bool isDir(std::filesystem::path const& p) {
        std::error_code ec;
        return std::filesystem::is_directory(p, ec);
    }

    void fillDir(Files& v, std::filesystem::path const& dir) {
        v.clear();
        if (!isDir(dir)) return;
        std::error_code ec;
        for (auto const& e : std::filesystem::directory_iterator(dir, ec)) {
            std::error_code e2;
            if (!e.is_regular_file(e2)) continue;
            std::string ext = lower(e.path().extension().string());
            if (ext == ".ogg" || ext == ".wav") v.push_back(e.path().string());
        }
        std::sort(v.begin(), v.end());
    }

    void fillPlayer(Pack& p, std::filesystem::path const& d) {
        fillDir(p.clicks,   d / "clicks");
        fillDir(p.releases, d / "releases");
        fillDir(p.soft,     d / "softclicks");
        fillDir(p.softRel,  d / "softreleases");
        fillDir(p.hard,     d / "hardclicks");
        fillDir(p.hardRel,  d / "hardreleases");
        fillDir(p.micro,    d / "microclicks");
        fillDir(p.microRel, d / "microreleases");
    }

    bool isPack(std::filesystem::path const& d) {
        return isDir(d / "player1") || isDir(d / "clicks");
    }

    void loadPack(std::filesystem::path const& r) {
        auto p1 = r / "player1";
        auto p2 = r / "player2";
        bool split = isDir(p1);
        fillPlayer(g_pack[0], split ? p1 : r);
        if (split && isDir(p2)) fillPlayer(g_pack[1], p2);
        else g_pack[1] = g_pack[0];
    }

    void scan() {
        g_names.clear();
        g_pack[0] = Pack{};
        g_pack[1] = Pack{};

        std::error_code ec;
        std::filesystem::create_directories(root(), ec);
        if (isPack(root())) g_names.push_back("");
        for (auto const& e : std::filesystem::directory_iterator(root(), ec)) {
            std::error_code e2;
            if (!e.is_directory(e2)) continue;
            if (isPack(e.path())) g_names.push_back(e.path().filename().string());
        }
        std::sort(g_names.begin(), g_names.end());

        if (!g_names.empty()) {
            int i = std::clamp(cky::clickPack, 0, static_cast<int>(g_names.size()) - 1);
            loadPack(g_names[i].empty() ? root() : root() / g_names[i]);
        }
        g_scanned = true;
    }

    Files const* firstNonEmpty(std::initializer_list<Files const*> l) {
        for (auto p : l) {
            if (p && !p->empty()) return p;
        }
        return nullptr;
    }
}

void ckyClickRescan() { g_scanned = false; }

std::string ckyClickPackName() {
    if (!g_scanned) scan();
    if (g_names.empty()) return "none";
    int i = std::clamp(cky::clickPack, 0, static_cast<int>(g_names.size()) - 1);
    return g_names[i].empty() ? "root" : g_names[i];
}

void ckyClickPackStep(int dir) {
    scan();
    if (g_names.empty()) {
        ckyNotify("No clickpack found in game/clicks", false);
        return;
    }
    int n = static_cast<int>(g_names.size());
    cky::clickPack = ((cky::clickPack + dir) % n + n) % n;
    scan();
}

void ckyClickSound(bool down, bool isPlayer1) {
    if (!cky::clickbotOn) return;
    if (!g_scanned) scan();

    int pi = isPlayer1 ? 0 : 1;
    Pack const& pk = g_pack[pi];
    double t = cky::frameClock / static_cast<double>(std::max(1.f, cky::macroTps));

    int kind;
    if (down) {
        double gap = t - g_lastPress[pi];
        if (gap < 0.0 || gap >= HARD_GAP) kind = HARD;
        else if (gap >= NORMAL_GAP) kind = NORMAL;
        else if (gap >= SOFT_GAP) kind = SOFT;
        else kind = MICRO;
        g_lastPress[pi] = t;
        g_lastKind[pi] = kind;
    } else {
        kind = g_lastKind[pi];
    }

    Files const* pool = nullptr;
    if (down) {
        switch (kind) {
        case HARD:  pool = firstNonEmpty({ &pk.hard, &pk.clicks }); break;
        case SOFT:  pool = firstNonEmpty({ &pk.soft, &pk.clicks }); break;
        case MICRO: pool = firstNonEmpty({ &pk.micro, &pk.soft, &pk.clicks }); break;
        default:    pool = firstNonEmpty({ &pk.clicks }); break;
        }
    } else {
        switch (kind) {
        case HARD:  pool = firstNonEmpty({ &pk.hardRel, &pk.releases, &pk.clicks }); break;
        case SOFT:  pool = firstNonEmpty({ &pk.softRel, &pk.releases, &pk.clicks }); break;
        case MICRO: pool = firstNonEmpty({ &pk.microRel, &pk.softRel, &pk.releases, &pk.clicks }); break;
        default:    pool = firstNonEmpty({ &pk.releases, &pk.clicks }); break;
        }
    }
    if (!pool) return;

    auto engine = FMODAudioEngine::sharedEngine();
    if (!engine) return;
    std::uniform_int_distribution<size_t> d(0, pool->size() - 1);
    engine->playEffect((*pool)[d(g_rng)], 1.f, 1.f, 1.f);
}

// END OF FILE (clickbot.cpp)
