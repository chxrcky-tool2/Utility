#pragma once
#include <concepts>
#include <filesystem>
#include <string>
#include <type_traits>
#include <asp/fs/std.hpp>

namespace cky {

    template <class P>
    inline std::filesystem::path stdPath(P const& p) {
        using Std = std::filesystem::path;
        if constexpr (std::is_same_v<std::remove_cvref_t<P>, Std>) {
            return p;
        } else if constexpr (requires { p.toStdPath(); }) {
            auto r = p.toStdPath();
            using R = std::remove_cvref_t<decltype(r)>;
            if constexpr (std::is_same_v<R, Std>) {
                return r;
            } else if constexpr (requires { r.unwrapOr(Std{}); }) {
                return r.unwrapOr(Std{});
            } else if constexpr (requires { r.unwrap(); }) {
                return r.unwrap();
            } else {
                return Std(std::string(p.string()));
            }
        } else {
            return Std(std::string(p.string()));
        }
    }
}
