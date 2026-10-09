#pragma once

#include "core/Vec3.h"
#include "modes/GameMode.h"

#include <array>
#include <vector>

namespace cultulhu {

// 5v5 capture-the-point: teams fight over capture points; owning a point
// scores ticks over time.
//
// Wave 21 tuning (match lasts ~8-12 min in bot play):
//   TARGET_SCORE = 300; TICK_INTERVAL = 2.0 s; 1 pt per owned point per tick.
//   Holding all 3 points = 90 pts/min (3.3 min to win); holding 2 = 60/min
//   (5 min); contested play lands between, capped by the 600 s time limit.
//   At the time limit the higher score wins; a tie is a draw (winner -1).
//   MatchEnded still fires via noteMatchTick.
class CapturePointMode : public GameMode {
public:
    static constexpr float TARGET_SCORE = 300.0f;
    static constexpr double TICK_INTERVAL = 2.0;   // scoring tick seconds
    static constexpr float CAPTURE_RATE = 0.25f;  // progress per sec per net occupant
    static constexpr double TIME_LIMIT = 600.0;   // seconds; then higher score wins

    CapturePointMode(EventBus& bus, GameClock& clock) : GameMode(bus, clock) {}

    void addPoint(Vec3 pos, float radius = 10.0f);
    // Wave 21: the standard 5v5 map — 3 points on eldritch_battlefield
    // zone centers (radius 10): shattered_court (0,0,0),
    // cavern_mouth (140,0,0), ember_shrine (0,0,140).
    void setupDefaultPoints();
    // Test/gameplay hook: how many units of each team stand on a point.
    void setOccupants(size_t pointIdx, int team0, int team1);

    void update(double dt) override;
    bool isOver() const override;
    int winner() const override;

    float score(int team) const { return score_[team]; }
    int pointOwner(size_t i) const { return points_.at(i).owner; }
    Vec3 pointPos(int i) const { return points_.at(i).pos; }
    float pointRadius(int i) const { return points_.at(i).radius; }
    float pointProgress(int i, int team) const { return points_.at(i).progress[team]; }
    size_t pointCount() const { return points_.size(); }
    double elapsed() const { return elapsed_; }

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
    double elapsed_ = 0.0;
};

} // namespace cultulhu
