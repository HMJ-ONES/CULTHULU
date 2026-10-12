// CULT-ULHU wave 42: match action logs.

#include "modes/MatchLogger.h"
#include "modes/Match.h"
#include "modes/RosterUtils.h"

#include <cmath>
#include <cstdio>
#include <ctime>

namespace cultulhu {

std::string MatchLogger::fmtT(double t) {
    const int ti = static_cast<int>(t);
    char b[16];
    std::snprintf(b, sizeof(b), "[%02d:%02d]", ti / 60, ti % 60);
    return b;
}

std::string MatchLogger::fmtPos(const Vec3& v) {
    char b[32];
    std::snprintf(b, sizeof(b), "(%d,%d)", static_cast<int>(v.x),
                  static_cast<int>(v.z));
    return b;
}

std::string MatchLogger::fmtDir(float yaw) {
    int deg = static_cast<int>(yaw * 57.29578f);
    while (deg < 0) deg += 360;
    while (deg >= 360) deg -= 360;
    static const char* c8[8] = {"E", "NE", "N", "NW", "W", "SW", "S", "SE"};
    return std::string(c8[((deg + 22) % 360) / 45]) + " " +
           std::to_string(deg) + "deg";
}

std::string MatchLogger::shortKit(const std::string& kitId) {
    const auto p = kitId.find('_');
    std::string s =
        (p == std::string::npos) ? kitId : kitId.substr(0, p);
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(s[0]));
    return s;
}

std::string MatchLogger::logPath(const std::string& mode, uint64_t seed) {
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char b[128];
    std::snprintf(b, sizeof(b), "match-logs/%04d-%02d-%02d/%02d%02d%02d-%s-%llu.log",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour,
                  tmv.tm_min, tmv.tm_sec, mode.c_str(),
                  static_cast<unsigned long long>(seed));
    return b;
}

void MatchLogger::attach(Match& match) {
    events_.clear();
    lastHolder_ = -2;
    lastScore0_ = lastScore1_ = -1;

    // Find our player index by id (stable for the match).
    match.bus().subscribe(EventType::PlayerKilled,
        [this, &match](const GameEvent& e) {
            const Match::Player* k = match.findPlayer(e.sourceId);
            const Match::Player* v = match.findPlayer(e.targetId);
            std::string ks = k ? (k->name + "<" + shortKit(k->kitId) + ">t" +
                                  std::to_string(k->team))
                               : "minions";
            std::string vs = v ? (v->name + "<" + shortKit(v->kitId) + ">t" +
                                  std::to_string(v->team))
                               : "?";
            std::string line = "KILL " + ks;
            if (k) line += " " + fmtPos(k->pos) + " " + fmtDir(k->facingYaw);
            line += " >> " + vs;
            if (v) line += " " + fmtPos(v->pos);
            events_.push_back({match.elapsed(), "KILL", line});
        });

    match.bus().subscribe(EventType::AbilityCast,
        [this, &match](const GameEvent& e) {
            // Wave 42: Cthulhu's rock casts are landmark events — always log
            // them. Other RMB spam stays out of the log.
            const bool isRock =
                e.tag == "Sunder the Earth" || e.tag == "Hurl the Mountain" ||
                e.tag == "the rock crumbles to dust" ||
                e.tag == "the rock shatters";
            if (e.amount > 2 && !isRock) return;
            const Match::Player* c = match.findPlayer(e.sourceId);
            if (!c) return;
            static const char* slot[4] = {"Q", "F", "R", "RMB"};
            const int s = static_cast<int>(e.amount);
            if (s < 0 || s > 3) return;
            events_.push_back(
                {match.elapsed(), "CAST",
                 "CAST " + c->name + "<" + shortKit(c->kitId) + ">t" +
                     std::to_string(c->team) + " " + slot[s] + " \"" + e.tag +
                     "\" " + fmtDir(c->facingYaw) + " " + fmtPos(c->pos)});
        });

    match.bus().subscribe(EventType::AbilityUnlocked,
        [this, &match](const GameEvent& e) {
            const Match::Player* p = match.findPlayer(e.sourceId);
            if (!p) return;
            events_.push_back(
                {match.elapsed(), "UNLOCK",
                 "UNLOCK " + p->name + "t" + std::to_string(p->team) +
                     " -> " + e.tag});
        });

    match.bus().subscribe(EventType::RelicClaimed,
        [this, &match](const GameEvent& e) {
            const Match::Player* p = match.findPlayer(e.sourceId);
            std::string who = p ? (p->name + "t" + std::to_string(p->team))
                                : "?";
            events_.push_back(
                {match.elapsed(), "RELIC",
                 "RELIC " + who + " claimed \"" + e.tag + "\""});
        });

    match.bus().subscribe(EventType::RelicManifested,
        [this, &match](const GameEvent& e) {
            events_.push_back({match.elapsed(), "RELIC",
                               "OMEN mythic relic manifests: \"" + e.tag +
                                   "\""});
        });

    // 11:30 AM convenience pass: kill streaks are the match's most
    // exciting beats but never reached the exported log — the GitHub
    // story skipped DOUBLE KILL / RAMPAGE entirely.
    match.bus().subscribe(EventType::KillStreak,
        [this, &match](const GameEvent& e) {
            const Match::Player* p = match.findPlayer(e.sourceId);
            std::string who = p ? (p->name + "<" + shortKit(p->kitId) + ">t" +
                                   std::to_string(p->team))
                                : e.tag;
            const int s = static_cast<int>(e.amount);
            const char* label = (s == 2)   ? "DOUBLE KILL"
                                : (s == 3) ? "TRIPLE KILL"
                                : (s == 4) ? "QUAD KILL"
                                           : "RAMPAGE";
            events_.push_back(
                {match.elapsed(), "STREAK",
                 "STREAK " + who + " — " + label + " (" +
                     std::to_string(s) + ")"});
        });

    // Micro-dev (2026-10-12): the match-point callout is the match's
    // climax beat — the exported story should carry it, not just SCORE
    // milestones.
    match.bus().subscribe(EventType::MatchPoint,
        [this, &match](const GameEvent& e) {
            const int toWin = static_cast<int>(
                CapturePointMode::TARGET_SCORE - e.amount);
            events_.push_back(
                {match.elapsed(), "MATCHPOINT",
                 "MATCHPOINT team " + std::to_string(e.faction) + " — " +
                     std::to_string(toWin) + " points from victory"});
        });
}

void MatchLogger::poll(Match& match) {
    if (match.capture()) {
        const int h = match.capture()->holder();
        if (h != lastHolder_) {
            events_.push_back(
                {match.elapsed(), "POINT",
                 h < 0 ? "POINT neutral — nobody holds the center"
                       : "POINT team " + std::to_string(h) +
                             " seizes the point"});
            lastHolder_ = h;
        }
        const int s0 = static_cast<int>(match.capture()->score(0));
        const int s1 = static_cast<int>(match.capture()->score(1));
        if (s0 / 100 != lastScore0_ / 100 || s1 / 100 != lastScore1_ / 100) {
            events_.push_back(
                {match.elapsed(), "SCORE",
                 "SCORE " + std::to_string(s0) + "-" + std::to_string(s1)});
            lastScore0_ = s0;
            lastScore1_ = s1;
        }
    }
}

std::string MatchLogger::render(const Match& match) const {
    std::string out;
    out += "# CULT-ULHU match log\n";
    // Wave 42: user-facing mode names in the log header (not internals).
    const std::string mode = match.modeName() == "capture" ? "domination"
                             : match.modeName() == "moba"  ? "dotgoo"
                                                            : match.modeName();
    out += "# mode: " + mode + "\n";
    char wb[64];
    std::snprintf(wb, sizeof(wb), "# winner: team %d\n", match.winner());
    out += wb;
    if (match.capture()) {
        std::snprintf(wb, sizeof(wb), "# final score: %d-%d (target %d)\n",
                      static_cast<int>(match.capture()->score(0)),
                      static_cast<int>(match.capture()->score(1)),
                      static_cast<int>(CapturePointMode::TARGET_SCORE));
        out += wb;
    }
    if (match.moba()) {
        std::snprintf(wb, sizeof(wb), "# final GOO hp: %d / %d\n",
                      static_cast<int>(match.moba()->baseHp(0)),
                      static_cast<int>(match.moba()->baseHp(1)));
        out += wb;
    }
    out += "# roster:\n";
    auto rows = match.stats().rows();
    for (size_t i = 0; i < match.playerCount(); ++i) {
        const auto& p = match.player(i);
        std::snprintf(
            wb, sizeof(wb), "#   %s<%s> team %d KDA %d/%d/%d level %d\n",
            p.name.c_str(), shortKit(p.kitId).c_str(), p.team,
            i < rows.size() ? rows[i].kills : 0,
            i < rows.size() ? rows[i].deaths : 0,
            i < rows.size() ? rows[i].assists : 0, p.level);
        out += wb;
    }
    out += "# events:\n";
    for (const auto& e : events_) out += fmtT(e.t) + " " + e.text + "\n";
    out += "# end of log (" + std::to_string(events_.size()) + " events)\n";
    return out;
}

} // namespace cultulhu
