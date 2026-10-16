#pragma once

#include <cstdint>
#include <random>

namespace cultulhu {

// Deterministic random number generator (seeded; gameplay systems use this so
// runs are reproducible in tests).
class RNG {
public:
    explicit RNG(uint64_t seed = 0x9E3779B97F4A7C15ULL);

    // Uniform float in [a, b].
    float uniform(float a, float b);
    // True with probability p (p in [0,1]).
    bool chance(float p);
    // Uniform int in [a, b] (inclusive).
    int intRange(int a, int b);

private:
    std::mt19937_64 eng_;
};

} // namespace cultulhu
