#include "modes/CapturePointMode.h"

namespace cultulhu {

void CapturePointMode::addPoint(Vec3 pos, float radius) {
    Point p;
    p.pos = pos;
    p.radius = radius;
    points_.push_back(p);
}

void CapturePointMode::setupDominationPoint() {
    points_.clear();
    addPoint(Vec3(0, 0, 0), POINT_RADIUS); // shattered_court, map center
}

void CapturePointMode::setOccupants(size_t pointIdx, int team0, int team1) {
    Point& p = points_.at(pointIdx);
    p.occupants[0] = team0;
    p.occupants[1] = team1;
}

void CapturePointMode::notePlayerKill(int killerTeam) {
    if (killerTeam == 0 || killerTeam == 1) score_[killerTeam] += KILL_POINTS;
}

int CapturePointMode::currentHolder() const {
    if (points_.empty()) return -1;
    const Point& p = points_[0];
    if (p.occupants[0] > 0 && p.occupants[1] == 0) return 0;
    if (p.occupants[1] > 0 && p.occupants[0] == 0) return 1;
    return -1; // empty or contested: nobody banks
}

bool CapturePointMode::contested() const {
    if (points_.empty()) return false;
    const Point& p = points_[0];
    return p.occupants[0] > 0 && p.occupants[1] > 0;
}

void CapturePointMode::update(double dt) {
    // Overtime begins the instant a tied clock expires; noteMatchTick runs
    // LAST so isOver()/winner() see this tick's state (a time-limit expiry
    // or overtime-deciding score inside this update still ends the match).
    elapsed_ += dt;
    // The transition tick itself must not count as overtime: otherwise a
    // single huge dt (or the exact TIME_LIMIT tick) would instantly burn
    // through OVERTIME_LIMIT and end the match as a draw.
    bool freshOvertime = false;
    if (!overtime_ && elapsed_ >= TIME_LIMIT &&
        score_[0] == score_[1]) {
        overtime_ = true;
        freshOvertime = true;
        // Micro-dev (2026-10-12): the tie at the clock expiring used to
        // flip the mode silently — match status showed "OVERTIME" but the
        // console and the exported log missed the drama. Announce once.
        GameEvent e(EventType::OvertimeStarted);
        bus_.publish(e);
    }

    int holder = currentHolder();
    if (holder != holder_) {
        const int prevHolder = holder_;
        holder_ = holder;
        if (holder_ >= 0 && !points_.empty()) {
            GameEvent e(EventType::PointCaptured);
            e.targetId = 0;
            e.faction = holder_;
            e.amount = 1.0f;
            e.pos = points_[0].pos;
            bus_.publish(e);
        } else if (prevHolder >= 0 && contested()) {
            // 2 PM build (2026-10-15): the team->contested flip used to
            // flip the bank silently — the point stopped ticking with no
            // event and no MatchLogger line, so the exported story could
            // never answer "when did we lose the point." One beat per
            // flip: holder_ staying -1 across ticks guards
            // contested->contested, the prevHolder>=0 check skips
            // neutral->contested (nobody lost a bank), and
            // contested()->team is already covered by PointCaptured.
            GameEvent e(EventType::PointContested);
            e.targetId = 0;
            e.faction = prevHolder; // the team that lost the bank
            e.amount = 1.0f;
            e.pos = points_[0].pos;
            bus_.publish(e);
        }
    }
    if (holder_ >= 0) score_[holder_] += HOLD_POINTS_PER_SEC * static_cast<float>(dt);

    // Micro-dev (2026-10-12): match point — once per team. Kills are fed by
    // Match before mode_->update, so kill-driven crossings land here too.
    for (int t = 0; t < 2; ++t) {
        if (!matchPoint_[t] && score_[t] >= MATCH_POINT_SCORE) {
            matchPoint_[t] = true;
            GameEvent e(EventType::MatchPoint);
            e.faction = t;
            e.amount = MATCH_POINT_SCORE;
            bus_.publish(e);
        }
    }

    if (overtime_ && !freshOvertime) overtimeElapsed_ += dt;
    noteMatchTick("capture");
}

bool CapturePointMode::isOver() const {
    if (score_[0] >= TARGET_SCORE || score_[1] >= TARGET_SCORE) return true;
    if (!overtime_ && elapsed_ >= TIME_LIMIT) return true;
    // Overtime: first score breaks the tie; the cap forces a draw.
    if (overtime_ && (score_[0] != score_[1] || overtimeElapsed_ >= OVERTIME_LIMIT))
        return true;
    return false;
}

int CapturePointMode::winner() const {
    if (score_[0] >= TARGET_SCORE) return 0;
    if (score_[1] >= TARGET_SCORE) return 1;
    if (!overtime_ && elapsed_ >= TIME_LIMIT) {
        if (score_[0] > score_[1]) return 0;
        if (score_[1] > score_[0]) return 1;
        return -1; // tie -> overtime handles it; unreachable in practice
    }
    if (overtime_) {
        if (score_[0] > score_[1]) return 0;
        if (score_[1] > score_[0]) return 1;
        return -1; // overtime cap expired tied: draw
    }
    return -1;
}

} // namespace cultulhu
