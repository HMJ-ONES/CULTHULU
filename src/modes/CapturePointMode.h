#pragma once

#include "core/Vec3.h"
#include "modes/GameMode.h"

#include <array>
#include <vector>

namespace cultulhu {

// 5v5 capture-the-point: teams fight over capture points; owning a point
// scores ticks over time. First team to TARGET_SCORE wins.
class CapturePointMode : public GameMode {
public:
    static constexpr float TARGET_SCORE = 100.0f;
    static constexpr double TICK_INTERVAL = 5.0;   // scoring tick seconds
    static constexpr float CAPTURE_RATE = 0.25f;  // progress per sec per net occupant

    CapturePointMode(EventBus& bus, GameClock& clock) : GameMode(bus, clock) {}

    void addPoint(Vec3 pos, float radius = 10.0f);
    // Test/gameplay hook: how many units of each team stand on a point.
    void setOccupants(size_t pointIdx, int team0, int team1);

    void update(double dt) override;
    bool isOver() const override;
    int winner() const override;

    float score(int team) const { return score_[team]; }
    int pointOwner(size_t i) const { return points_.at(i).owner; }
    size_t pointCount() const { return points_.size(); }

private:
    struct Point {
        Vec3 pos;
        float radius = 10.0f;
        int owner = -1;              // -1 = neutral
        float progress[2] = {0, 0}; // capture progress per team
        int occupants[2] = {0, 0};
    };

    std::vector<Point> points_;
    float score_[2] = {0.0f, 0.0f};
    double tickTimer_ = 0.0;
};

} // namespace cultulhu
