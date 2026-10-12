#pragma once

// CULT-ULHU wave 42: match action logs.
// A MatchLogger attaches to a Match's EventBus at match start, records the
// key events (kills, casts, unlocks, score/point swings, relic claims, the
// final result) with timestamps, positions, facings and kits, and renders
// a compact human-readable text log. The log is pushed to GitHub
// (match-logs/) at match end — never stored on the player's PC — and a
// daily cleanup job deletes logs older than 30 days.

#include "core/EventBus.h"
#include "core/Vec3.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cultulhu {

class Match;

struct LoggedEvent {
    double t = 0.0;          // match seconds
    std::string kind;        // "KILL" "CAST" "UNLOCK" "POINT" "SCORE" "RELIC" "STREAK" "END"
    std::string text;        // one human-readable line (without timestamp)
};

class MatchLogger {
public:
    // Attach to the match's event bus. Call once right after match start.
    void attach(Match& match);

    // Poll each tick for score/point changes (events don't cover those).
    void poll(Match& match);

    // Render the full log as compact text.
    std::string render(const Match& match) const;

    // Suggested GitHub path: match-logs/YYYY-MM-DD/HHMMSS-<mode>-<seed>.log
    static std::string logPath(const std::string& mode, uint64_t seed);

    const std::vector<LoggedEvent>& events() const { return events_; }

private:
    std::vector<LoggedEvent> events_;
    int lastHolder_ = -2;
    int lastScore0_ = -1;
    int lastScore1_ = -1;
    uint64_t seed_ = 0;

    static std::string fmtT(double t);
    static std::string fmtPos(const Vec3& v);
    static std::string fmtDir(float yaw);
    static std::string shortKit(const std::string& kitId);
};

} // namespace cultulhu
