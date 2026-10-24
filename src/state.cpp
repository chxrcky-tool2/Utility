#include "state.hpp"
#include <fstream>
#include <Geode/utils/file.hpp>

namespace cky {
    void saveMacro(const std::string& name) {
        std::string path = geode::Mod::get()->getSaveDir().string() + "/" + name + ".cky";
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return;

        file.write("CKY1", 4);
        uint32_t count = currentMacro.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));

        for (const auto& input : currentMacro) {
            file.write(reinterpret_cast<const char*>(&input), sizeof(MacroInput));
        }
    }

    bool loadMacro(const std::string& name) {
        std::string path = geode::Mod::get()->getSaveDir().string() + "/" + name + ".cky";
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        char header[4];
        file.read(header, 4);
        if (std::string(header, 4) != "CKY1") return false;

        uint32_t count = 0;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        currentMacro.clear();
        currentMacro.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            MacroInput input;
            file.read(reinterpret_cast<char*>(&input), sizeof(MacroInput));
            currentMacro.push_back(input);
        }
        playbackCursor = 0;
        return true;
    }
}
