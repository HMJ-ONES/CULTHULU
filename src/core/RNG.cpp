#include "core/RNG.h"

namespace cultulhu {

RNG::RNG(uint64_t seed) : eng_(seed) {}

float RNG::uniform(float a, float b) {
    std::uniform_real_distribution<float> d(a, b);
    return d(eng_);
}

bool RNG::chance(float p) {
    if (p <= 0.0f) return false;
    if (p >= 1.0f) return true;
    return uniform(0.0f, 1.0f) < p;
}

int RNG::intRange(int a, int b) {
    std::uniform_int_distribution<int> d(a, b);
    return d(eng_);
}

} // namespace cultulhu
