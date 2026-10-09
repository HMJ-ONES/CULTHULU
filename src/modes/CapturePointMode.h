#pragma once

#include "core/Vec3.h"
#include "modes/GameMode.h"

#include <cstddef>
#include <vector>

namespace cultulhu {

// 5v5 Onslaught (Paladins-style): ONE central capture point. A team banks
// hold points while it has living fighters on the point and the enemy has
// none — the moment an enemy steps in, the point is contested and ticking
// stops. Player kills also score. First team to TARGET_SCORE wins; at
// TIME_LIMIT the higher score wins; a tie goes to sudden-death overtime
// (first score wins, capped by OVERTIME_LIMIT, then a draw).
//
// Seizing uncontested control publishes PointCaptured (faction = the team
// taking the point) so the Standard-Bearer achievement keeps firing on a
// real event. Re-contesting (both teams present) publishes nothing.
//
// Tunables (documented): HOLD_POINTS_PER_SEC = 1 -> uncontested holding
// banks 60 pts/min, so a shutout wins in ~6.7 min; KILL_POINTS = 5 makes
// teamfights the faster path. TIME_LIMIT = 600 s caps every match.
class CapturePointMode : public GameMode {
public:
    static constexpr float TARGET_SCORE = 400.0f;
    static constexpr float HOLD_POINTS_PER_SEC = 1.0f; // per team while holding
    static constexpr float KILL_POINTS = 5.0f;         // per player kill
    static constexpr double TIME_LIMIT = 600.0;       // seconds
    static constexpr double OVERTIME_LIMIT = 120.0;   // sudden-death cap
    static constexpr float POINT_RADIUS = 10.0f;

    CapturePointMode(EventBus& bus, GameClock& clock) : GameMode(bus, clock) {}

    void addPoint(Vec3 pos, float radius = POINT_RADIUS);
    // The standard 5v5 map: one point at the middle of eldritch_battlefield
    // (shattered_court, 0,0,0).
    void setupOnslaughtPoint();
    // Test/gameplay hook: how many living units of each team stand on a point.
    void setOccupants(size_t pointIdx, int team0, int team1);

    void update(double dt) override;
    bool isOver() const override;
    int winner() const override;

    // GameMode hook: Match calls this for every player-vs-player kill.
    void notePlayerKill(int killerTeam) override;

    float score(int team) const { return score_[team]; }
    // Team currently banking hold points, or -1 when nobody holds
    // (empty point or contested).
    int holder() const { return holder_; }
    bool contested() const;
    bool overtime() const { return overtime_; }
    double overtimeElapsed() const { return overtimeElapsed_; }
    Vec3 pointPos(size_t i) const { return points_.at(i).pos; }
    float pointRadius(size_t i) const { return points_.at(i).radius; }
    size_t pointCount() const { return points_.size(); }
    double elapsed() const { return elapsed_; }

private:
    struct Point {
        Vec3 pos;
        float radius = POINT_RADIUS;
        int occupants[2] = {0, 0};
    };
    int currentHolder() const; // who would bank right now (-1 if none)

    std::vector<Point> points_;
    float score_[2] = {0.0f, 0.0f};
    int holder_ = -1;
    bool overtime_ = false;
    double overtimeElapsed_ = 0.0;
    double elapsed_ = 0.0;
};

} // namespace cultulhu
