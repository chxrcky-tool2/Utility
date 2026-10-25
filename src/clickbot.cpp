
#include "state.hpp"
#include <cctype>
#include <random>

namespace {
    std::vector<std::string> g_press, g_release;
    bool g_scanned = false;
    std::mt19937 g_rng{ std::random_device{}() };

    std::string lower(std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    void scan() {
        g_press.clear();
        g_release.clear();
        std::error_code ec;
        auto dir = geode::dirs::getGameDir() / "clicks";
        std::filesystem::create_directories(dir, ec);
        for (auto const& e : std::filesystem::directory_iterator(dir, ec)) {
            if (!e.is_regular_file(ec)) continue;
            std::string ext = lower(e.path().extension().string());
            if (ext != ".ogg" && ext != ".wav") continue;
            std::string name = lower(e.path().filename().string());
            if (name.rfind("release", 0) == 0) g_release.push_back(e.path().string());
            else if (name.rfind("press", 0) == 0) g_press.push_back(e.path().string());
        }
        g_scanned = true;
    }
}

void ckyClickRescan() { g_scanned = false; }

void ckyClickSound(bool down, bool) {
    if (!cky::clickbotOn) return;
    if (!g_scanned) scan();
    auto& v = down ? g_press : g_release;
    if (v.empty()) return;
    auto engine = FMODAudioEngine::sharedEngine();
    if (!engine) return;
    std::uniform_int_distribution<size_t> d(0, v.size() - 1);
    engine->playEffect(v[d(g_rng)], 1.f, 1.f, 1.f);
}

// END OF FILE (clickbot.cpp)
