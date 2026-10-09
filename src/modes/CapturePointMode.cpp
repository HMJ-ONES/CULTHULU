#include "modes/CapturePointMode.h"

namespace cultulhu {

void CapturePointMode::addPoint(Vec3 pos, float radius) {
    Point p;
    p.pos = pos;
    p.radius = radius;
    points_.push_back(p);
}

void CapturePointMode::setupDefaultPoints() {
    addPoint(Vec3(0, 0, 0), 10.0f);      // shattered_court
    addPoint(Vec3(140, 0, 0), 10.0f);    // cavern_mouth
    addPoint(Vec3(0, 0, 140), 10.0f);    // ember_shrine
}

void CapturePointMode::setOccupants(size_t pointIdx, int team0, int team1) {
    Point& p = points_.at(pointIdx);
    p.occupants[0] = team0;
    p.occupants[1] = team1;
}

void CapturePointMode::update(double dt) {
    // noteMatchTick runs LAST: isOver()/winner() must see this tick's
    // state (a time-limit expiry inside this update still ends the match).
    elapsed_ += dt;
    size_t pointIdx = 0;
    for (auto& p : points_) {
        int net = p.occupants[0] - p.occupants[1];
        if (net > 0) {
            p.progress[0] += CAPTURE_RATE * static_cast<float>(dt) * net;
            p.progress[1] = 0.0f;
            if (p.progress[0] >= 1.0f) {
                // Wave 16: only a real takeover counts as a capture.
                if (p.owner != 0) {
                    p.owner = 0;
                    GameEvent e(EventType::PointCaptured);
                    e.targetId = pointIdx;
                    e.faction = 0;
                    e.amount = 1.0f;
                    e.pos = p.pos;
                    bus_.publish(e);
                }
                p.progress[0] = p.progress[1] = 0.0f;
            }
        } else if (net < 0) {
            p.progress[1] += CAPTURE_RATE * static_cast<float>(dt) * (-net);
            p.progress[0] = 0.0f;
            if (p.progress[1] >= 1.0f) {
                if (p.owner != 1) {
                    p.owner = 1;
                    GameEvent e(EventType::PointCaptured);
                    e.targetId = pointIdx;
                    e.faction = 1;
                    e.amount = 1.0f;
                    e.pos = p.pos;
                    bus_.publish(e);
                }
                p.progress[0] = p.progress[1] = 0.0f;
            }
        } else {
            p.progress[0] = p.progress[1] = 0.0f; // contested: no progress
        }
        ++pointIdx;
    }

    tickTimer_ += dt;
    while (tickTimer_ >= TICK_INTERVAL) {
        tickTimer_ -= TICK_INTERVAL;
        for (const auto& p : points_)
            if (p.owner == 0 || p.owner == 1) score_[p.owner] += 1.0f;
    }
    noteMatchTick("capture");
}

bool CapturePointMode::isOver() const {
    return score_[0] >= TARGET_SCORE || score_[1] >= TARGET_SCORE ||
           elapsed_ >= TIME_LIMIT;
}

int CapturePointMode::winner() const {
    if (score_[0] >= TARGET_SCORE) return 0;
    if (score_[1] >= TARGET_SCORE) return 1;
    if (elapsed_ >= TIME_LIMIT) {
        if (score_[0] > score_[1]) return 0;
        if (score_[1] > score_[0]) return 1;
        return -1; // draw
    }
    return -1;
}

} // namespace cultulhu
