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
    // 8 PM bug-fix (2026-10-12): drop stale subscriptions first — a second
    // attach() in one session (new `match`) must not accumulate handlers.
    detach();
    events_.clear();
    lastHolder_ = -2;
    lastScore0_ = lastScore1_ = -1;
    // Micro-dev (2026-10-13): seed the level map so gate-crossing
    // detection has a baseline for players already in the match.
    lastLevel_.clear();
    for (size_t i = 0; i < match.playerCount(); ++i)
        lastLevel_[match.player(i).id] = match.player(i).level;

    EventBus& bus = match.bus();
    bus_ = &bus;
    // Collect every subscription handle so detach() can remove them.
    auto sub = [&](EventType t, EventBus::Handler h) {
        subs_.push_back(bus.subscribe(t, std::move(h)));
    };

    // Micro-dev (2026-10-12): the exported story had no opening beat —
    // it jumped straight to KILL lines. MatchStarted fires on the first
    // update tick (GameMode::noteMatchTick), i.e. after attach(), so the
    // logger always catches it. The tag uses the same capture/moba
    // convention as the driver's MatchEnded console subscriber.
    sub(EventType::MatchStarted,
        [this, &match](const GameEvent& e) {
            const std::string modeName = (e.tag == "capture")   ? "Domination"
                                         : (e.tag == "moba")    ? "DOTGOO"
                                                               : e.tag;
            events_.push_back({match.elapsed(), "START",
                               "START " + modeName + " (5v5)"});
        });

    // Find our player index by id (stable for the match).
    sub(EventType::PlayerKilled,
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

    // Micro-dev (2026-10-12): point captures left no trace in the exported
    // story — the score moved with no record of who seized it. One line
    // per capture keeps the story honest.
    sub(EventType::PointCaptured,
        [this, &match](const GameEvent& e) {
            events_.push_back({match.elapsed(), "CAPTURE",
                               "CAPTURE t" + std::to_string(e.faction) +
                                   " seized the point"});
        });

    // Micro-dev (2026-10-12): first blood is the match's opening beat —
    // the exported story should carry it, not just the KILL line.
    sub(EventType::FirstBlood,
        [this, &match](const GameEvent& e) {
            const Match::Player* k = match.findPlayer(e.sourceId);
            const Match::Player* v = match.findPlayer(e.targetId);
            const std::string ks = k ? k->name : "the minions";
            const std::string vs = v ? v->name : "?";
            events_.push_back(
                {match.elapsed(), "FIRSTBLOOD",
                 "FIRSTBLOOD " + ks + " struck down " + vs});
        });

    // Micro-dev (2026-10-12): the dead used to come back with no trace in
    // the exported story — the DEAD row just vanished from `match status`.
    // Name the return so the log reads as a story with a second act.
    sub(EventType::PlayerRespawned,
        [this, &match](const GameEvent& e) {
            const Match::Player* p = match.findPlayer(e.sourceId);
            const std::string ps = p ? p->name : "?";
            const int team = p ? p->team : e.faction;
            events_.push_back({match.elapsed(), "RESPAWNED",
                               "RESPAWNED " + ps + " t" +
                                   std::to_string(team)});
        });

    // Micro-dev (2026-10-12): DOTGOO tower falls used to leave no trace
    // in the exported match log — the story jumped from lanes pushing
    // to the GOO fight with no record of a crumbled defense.
    sub(EventType::TowerDestroyed,
        [this, &match](const GameEvent& e) {
            const Match::Player* k = match.findPlayer(e.sourceId);
            const std::string ks =
                k ? (k->name + "<" + shortKit(k->kitId) + ">t" +
                     std::to_string(k->team))
                  : "minions";
            events_.push_back(
                {match.elapsed(), "TOWER",
                 "TOWER t" + std::to_string(e.faction) + " " + e.tag +
                     " down << " + ks});
        });

    // Micro-dev (2026-10-12): the GOO taking damage left no trace in the
    // exported log — the story jumped from lanes pushing to a dead god.
    sub(EventType::GreatOldOneUnderAttack,
        [this, &match](const GameEvent& e) {
            const Match::Player* a = match.findPlayer(e.sourceId);
            const std::string as =
                a ? (a->name + "<" + shortKit(a->kitId) + ">t" +
                     std::to_string(a->team))
                  : "minions";
            const int pct = static_cast<int>(e.amount * 100.0 + 0.5);
            events_.push_back(
                {match.elapsed(), "GOOATTACK",
                 "GOOATTACK t" + std::to_string(e.faction) + " " +
                     std::to_string(pct) + "% << " + as});
        });

    // Micro-dev (2026-10-13): the first-scratch tower warning left no trace
    // in the exported log — the story jumped from lane pushing to a fallen
    // tower. TOWERATTACK lands next to TOWER with the team, lane, HP %, and
    // the attacker attribution.
    sub(EventType::TowerUnderAttack,
        [this, &match](const GameEvent& e) {
            const Match::Player* a = match.findPlayer(e.sourceId);
            const std::string as =
                a ? (a->name + "<" + shortKit(a->kitId) + ">t" +
                     std::to_string(a->team))
                  : "minions";
            const int pct = static_cast<int>(e.amount * 100.0 + 0.5);
            events_.push_back(
                {match.elapsed(), "TOWERATTACK",
                 "TOWERATTACK t" + std::to_string(e.faction) + " " + e.tag +
                     " " + std::to_string(pct) + "% << " + as});
        });

    // Micro-dev (2026-10-12): the killing blow left no trace in the
    // exported log — the story ended at the GOOATTACK warnings and the
    // VICTORY line with no record of the god falling. Name the god's
    // death and the killer so the story has an ending.
    sub(EventType::GreatOldOneSlain,
        [this, &match](const GameEvent& e) {
            const Match::Player* a = match.findPlayer(e.sourceId);
            const std::string as =
                a ? (a->name + "<" + shortKit(a->kitId) + ">t" +
                     std::to_string(a->team))
                  : "minions";
            events_.push_back(
                {match.elapsed(), "GOOSLAIN",
                 "GOOSLAIN t" + std::to_string(e.faction) +
                     " falls << " + as});
        });

    sub(EventType::AbilityCast,
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

    sub(EventType::AbilityUnlocked,
        [this, &match](const GameEvent& e) {
            const Match::Player* p = match.findPlayer(e.sourceId);
            if (!p) return;
            events_.push_back(
                {match.elapsed(), "UNLOCK",
                 "UNLOCK " + p->name + "t" + std::to_string(p->team) +
                     " -> " + e.tag});
        });

    sub(EventType::RelicClaimed,
        [this, &match](const GameEvent& e) {
            const Match::Player* p = match.findPlayer(e.sourceId);
            std::string who = p ? (p->name + "t" + std::to_string(p->team))
                                : "?";
            events_.push_back(
                {match.elapsed(), "RELIC",
                 "RELIC " + who + " claimed \"" + e.tag + "\""});
        });

    sub(EventType::RelicManifested,
        [this, &match](const GameEvent& e) {
            events_.push_back({match.elapsed(), "RELIC",
                               "OMEN mythic relic manifests: \"" + e.tag +
                                   "\""});
        });

    // Micro-dev (2026-10-12): the 4s/6s channel on tier-2/3 jungle relics
    // was invisible — the story jumped from manifestation straight to
    // the claim with no record of the contest. Log each channel start.
    sub(EventType::RelicChannelStarted,
        [this, &match](const GameEvent& e) {
            const Match::Player* p = match.findPlayer(e.sourceId);
            std::string who = p ? (p->name + "t" + std::to_string(p->team))
                                : "?";
            events_.push_back(
                {match.elapsed(), "RELIC",
                 "CHANNEL " + who + " starts channeling \"" + e.tag +
                     "\" (tier " +
                     std::to_string(static_cast<int>(e.amount)) + ")"});
        });

    // Micro-dev (2026-10-13): an interrupted channel left no trace — the
    // story jumped from "starts channeling" to the next claim with no
    // record of the contest breaking it. One line per break.
    sub(EventType::RelicChannelInterrupted,
        [this, &match](const GameEvent& e) {
            const Match::Player* p = match.findPlayer(e.sourceId);
            std::string who = p ? (p->name + "t" + std::to_string(p->team))
                                : "?";
            events_.push_back(
                {match.elapsed(), "RELIC",
                 "INTERRUPT " + who + " channel broken on \"" + e.tag +
                     "\" (" +
                     std::to_string(static_cast<int>(e.amount + 0.5f)) +
                     "s invested)"});
        });

    // Micro-dev (2026-10-13): a stun/root landing was invisible in the
    // exported story — the reader saw a fighter go quiet with no
    // explanation. One CC line per application.
    sub(EventType::CCApplied,
        [this, &match](const GameEvent& e) {
            const Match::Player* p = match.findPlayer(e.sourceId);
            std::string who = p ? (p->name + "t" + std::to_string(p->team))
                                : "?";
            events_.push_back(
                {match.elapsed(), "CC",
                 "CC " + who + " held fast " +
                     std::to_string(static_cast<int>(e.amount + 0.5)) + "s"});
        });

    // 11:30 AM convenience pass: kill streaks are the match's most
    // exciting beats but never reached the exported log — the GitHub
    // story skipped DOUBLE KILL / RAMPAGE entirely.
    sub(EventType::KillStreak,
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

    // Micro-dev (2026-10-12): a shutdown deserves the same export treatment
    // as the streaks themselves — the GitHub story gets an ending for the
    // streak, not just the streak.
    sub(EventType::StreakEnded,
        [this, &match](const GameEvent& e) {
            const Match::Player* k = match.findPlayer(e.sourceId);
            const Match::Player* v = match.findPlayer(e.targetId);
            std::string ks = k ? (k->name + "<" + shortKit(k->kitId) + ">t" +
                                  std::to_string(k->team))
                               : (e.tag.empty() ? "the minions" : e.tag);
            std::string vs = v ? (v->name + "<" + shortKit(v->kitId) + ">t" +
                                  std::to_string(v->team))
                               : std::to_string(e.targetId);
            const int s = static_cast<int>(e.amount);
            const char* label = (s == 3)   ? "TRIPLE KILL"
                                : (s == 4) ? "QUAD KILL"
                                           : "RAMPAGE";
            events_.push_back(
                {match.elapsed(), "SHUTDOWN",
                 "SHUTDOWN " + ks + " ends " + vs + "'s " + label + " (" +
                     std::to_string(s) + ")"});
        });

    // Micro-dev (2026-10-12): the match-point callout is the match's
    // climax beat — the exported story should carry it, not just SCORE
    // milestones.
    sub(EventType::MatchPoint,
        [this, &match](const GameEvent& e) {
            const int toWin = static_cast<int>(
                CapturePointMode::TARGET_SCORE - e.amount);
            events_.push_back(
                {match.elapsed(), "MATCHPOINT",
                 "MATCHPOINT team " + std::to_string(e.faction) + " — " +
                     std::to_string(toWin) + " points from victory"});
        });

    // Micro-dev (2026-10-12): sudden death is the match's wildest beat —
    // the exported story should record the flip, not just trail off.
    sub(EventType::OvertimeStarted,
        [this, &match](const GameEvent&) {
            events_.push_back(
                {match.elapsed(), "OVERTIME",
                 "OVERTIME sudden death begins — first score wins"});
        });

    // Micro-dev (2026-10-13): Domination's opening ult charge completing
    // is a story beat — the moment each fighter's big button comes online.
    // The exported log should carry it, not just the KILL lines.
    sub(EventType::UltCharged,
        [this, &match](const GameEvent& e) {
            const Match::Player* p = match.findPlayer(e.sourceId);
            const std::string ps =
                p ? (p->name + "<" + shortKit(p->kitId) + ">t" +
                     std::to_string(p->team))
                  : "?";
            events_.push_back(
                {match.elapsed(), "ULTREADY",
                 "ULTREADY " + ps + " — " + e.tag + " charged"});
        });

    // Micro-dev (2026-10-13): the tiered-invocation gates at 10/20/30 are
    // the match's progression beats, but the exported story skipped them
    // entirely — the log jumped from streaks to fallen gods with no record
    // of a fighter unlocking tier-2 or tier-3 invocations. Log only the
    // gate crossings (30 lines max in a 10-player match, never per-level
    // noise); the crossing check catches multi-level jumps that land past
    // a gate, since PlayerLeveledUp only carries the final level.
    sub(EventType::PlayerLeveledUp,
        [this, &match](const GameEvent& e) {
            const int prev =
                lastLevel_.count(e.sourceId) ? lastLevel_[e.sourceId] : 1;
            const int lv = static_cast<int>(e.amount);
            lastLevel_[e.sourceId] = lv;
            const Match::Player* p = match.findPlayer(e.sourceId);
            const std::string ps =
                p ? (p->name + "<" + shortKit(p->kitId) + ">t" +
                     std::to_string(p->team))
                  : e.tag;
            for (int g = 10; g <= 30; g += 10) {
                if (prev < g && lv >= g) {
                    events_.push_back(
                        {match.elapsed(), "LEVEL",
                         "LEVEL " + ps + " hits level " +
                             std::to_string(g) + " — invocation tier " +
                             std::to_string(g / 10) + " unlocked"});
                }
            }
        });
}

void MatchLogger::detach() {
    if (!bus_) return;
    for (EventBus::Subscription s : subs_) bus_->unsubscribe(s);
    subs_.clear();
    bus_ = nullptr;
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
