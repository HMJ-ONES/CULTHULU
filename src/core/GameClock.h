#pragma once

namespace cultulhu {

// Game-time clock (seconds). All timers (adoption, cooldowns, rituals) run on
// game time so they can be fast-forwarded in tests.
class GameClock {
public:
    GameClock() = default;

    double now() const { return t_; }
    void advance(double dt) { t_ += dt; }
    void reset() { t_ = 0.0; }

private:
    double t_ = 0.0;
};

} // namespace cultulhu
