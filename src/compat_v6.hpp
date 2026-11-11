#pragma once
#include <filesystem>
#include <string>
#include <type_traits>
#include <asp/fs/std.hpp>

namespace cky {
    template <class P>
    inline std::filesystem::path stdPath(P const& p) {
        if constexpr (std::is_same_v<std::remove_cvref_t<P>, std::filesystem::path>) {
            return p;
        } else if constexpr (requires { p.toStdPath(); }) {
            return p.toStdPath();
        } else {
            return std::filesystem::path(std::string(p.string()));
        }
    }
}
