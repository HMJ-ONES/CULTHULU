#pragma once

namespace cultulhu {

// Cthulhu's power: 0..1000. BeliefSystem produces deltas via onEvent()/tick();
// this class just owns the value and clamps it.
class PowerSystem {
public:
    static constexpr float MAX_POWER = 1000.0f;
    static constexpr float START_POWER = 100.0f; // creative liberty (see README)

    PowerSystem() = default;

    float value() const { return power_; }
    void add(float delta);
    void set(float v);

private:
    float power_ = START_POWER;
};

} // namespace cultulhu
