#include "power/PowerSystem.h"

namespace cultulhu {

void PowerSystem::add(float delta) {
    power_ += delta;
    if (power_ > MAX_POWER) power_ = MAX_POWER;
    if (power_ < 0.0f) power_ = 0.0f;
}

void PowerSystem::set(float v) {
    power_ = v;
    if (power_ > MAX_POWER) power_ = MAX_POWER;
    if (power_ < 0.0f) power_ = 0.0f;
}

} // namespace cultulhu
