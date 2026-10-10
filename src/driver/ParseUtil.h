#pragma once
// Small parsing utilities for the driver REPL (CULT-ULHU).
//
// safeStoi: overflow-safe integer parse for digit tokens. std::stoi throws
// std::out_of_range on huge digit strings (fuzz-found via
// `spawn cultist 99999999999999999992`); this clamps to INT_MAX instead of
// throwing. Non-digit input yields 0.

#include <limits>
#include <string>

namespace cultulhu {
namespace driver {

inline int safeStoi(const std::string& t) {
    long long v = 0;
    for (char c : t) {
        if (c < '0' || c > '9') return 0;
        v = v * 10 + (c - '0');
        if (v >= std::numeric_limits<int>::max())
            return std::numeric_limits<int>::max();
    }
    return static_cast<int>(v);
}

} // namespace driver
} // namespace cultulhu
