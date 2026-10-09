#include "modes/Match.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cultulhu {

Match::Match(EventBus& bus, GameClock& clock, RNG& rng)
    : bus_(bus), clock_(clock), rng_(rng) {
    players_.reserve(kMaxPlayers);
    playerTargets_.reserve(kMaxPlayers);
    // Capture-mode spawn corners; moba overrides from its map bases.
    teamBase_[0] = Vec3(-80, 0, -80);
    teamBase_[1] = Vec3(80, 0, 80);
}

bool Match::start(const std::string& mode) {
    modeName_ = mode;
    players_.clear();
    stats_.clear();
    elapsed_ = 0.0;
    nextBotNum_ = 1;
    mode_.reset();
    capture_ = nullptr;
    moba_ = nullptr;
    if (mode == "capture") {
        auto m = std::make_unique<CapturePointMode>(bus_, clock_);
        m->setupDefaultPoints();
        capture_ = m.get();
        mode_ = std::move(m);
        teamBase_[0] = Vec3(-80, 0, -80);
        teamBase_[1] = Vec3(80, 0, 80);
        return true;
    }
    if (mode == "moba") {
        auto m = std::make_unique<MobaDefense>(bus_, clock_, rng_);
        m->setupDefaultMap();
        moba_ = m.get();
        mode_ = std::move(m);
        teamBase_[0] = moba_->basePos(0);
        teamBase_[1] = moba_->basePos(1);
        // Route minion/tower damage onto match players (deaths resolve
        // through Match so PlayerKilled / KDA stay correct).
        moba_->setPlayerDamageHook(
            [this](uint64_t targetId, float dmg, uint64_t attackerEntityId) {
                const Player* p = findPlayer(targetId);
                if (!p || !p->alive) return;
                size_t idx = 0;
                for (; idx < players_.size(); ++idx)
                    if (players_[idx].id == targetId) break;
                if (idx >= players_.size()) return;
                damagePlayer(idx, dmg);
                if (!players_[idx].alive) killPlayer(idx, -1, attackerEntityId);
            });
        return true;
    }
    return false;
}

int Match::countTeam(int team) const {
    int n = 0;
    for (const auto& p : players_)
        if (p.team == team) ++n;
    return n;
}

uint64_t Match::addPlayer(const std::string& name, int team) {
    if (players_.size() >= static_cast<size_t>(kMaxPlayers)) return 0;
    if (team < 0) team = (countTeam(0) <= countTeam(1)) ? 0 : 1;
    team = (team == 0) ? 0 : 1;
    Player p;
    p.id = kPlayerIdBase + players_.size();
    p.name = name.empty() ? ("Player" + std::to_string(p.id)) : name;
    p.team = team;
    p.isBot = false;
    p.lane = static_cast<int>(players_.size()) % 3;
    spawnPlayer(p);
    players_.push_back(p);
    stats_.setPlayerName(players_.size() - 1, p.name);
    return p.id;
}

uint64_t Match::addBot(int team) {
    if (players_.size() >= static_cast<size_t>(kMaxPlayers)) return 0;
    if (team < 0) team = (countTeam(0) <= countTeam(1)) ? 0 : 1;
    team = (team == 0) ? 0 : 1;
    Player p;
    p.id = kPlayerIdBase + players_.size();
    p.name = "Bot" + std::to_string(nextBotNum_++);
    p.team = team;
    p.isBot = true;
    p.lane = static_cast<int>(players_.size()) % 3;
    spawnPlayer(p);
    players_.push_back(p);
    stats_.setPlayerName(players_.size() - 1, p.name);
    return p.id;
}

void Match::botfill() {
    while (countTeam(0) < 5 && players_.size() < static_cast<size_t>(kMaxPlayers))
        addBot(0);
    while (countTeam(1) < 5 && players_.size() < static_cast<size_t>(kMaxPlayers))
        addBot(1);
}

void Match::spawnPlayer(Player& p) {
    // Scatter around the team base so 5 bots don't stack on one spot.
    float jx = static_cast<float>(rng_.uniform(-6, 6));
    float jz = static_cast<float>(rng_.uniform(-6, 6));
    p.pos = teamBase_[p.team] + Vec3(jx, 0, jz);
    p.hp = p.maxHp;
    p.alive = true;
    p.respawnTimer = 0.0;
    p.swingCd = 0.0;
    p.laneFrac = 0.0f;
}

const Match::Player* Match::findPlayer(uint64_t id) const {
    for (const auto& p : players_)
        if (p.id == id) return &p;
    return nullptr;
}

void Match::update(double dt) {
    if (!mode_) return;
    elapsed_ += dt;
    for (size_t i = 0; i < players_.size(); ++i) updatePlayer(players_[i], i, dt);
    if (capture_) {
        feedCaptureOccupants();
    } else if (moba_) {
        feedMobaPlayerTargets();
    }
    mode_->update(dt);
}

void Match::updatePlayer(Player& p, size_t idx, double dt) {
    if (p.swingCd > 0) p.swingCd -= dt;
    if (!p.alive) {
        p.respawnTimer -= dt;
        if (p.respawnTimer <= 0.0) spawnPlayer(p);
        return;
    }
    // An enemy in melee reach pins the bot: stand and fight instead of
    // walking off toward the objective.
    if (!enemyInMeleeRange(p, idx)) {
        if (capture_)
            updateCaptureObjective(p, dt);
        else if (moba_)
            updateMobaObjective(p, dt);
    }
    // Enemy players first; only then swing at moba structures/minions.
    bool attacked = attackNearestEnemy(p, idx, dt);
    if (moba_ && !attacked) swingAtStructures(p);
}

bool Match::enemyInMeleeRange(const Player& p, size_t idx) const {
    for (size_t i = 0; i < players_.size(); ++i) {
        if (i == idx) continue;
        const Player& o = players_[i];
        if (!o.alive || o.team == p.team) continue;
        if (p.pos.distance(o.pos) <= kMeleeRange) return true;
    }
    return false;
}

void Match::updateCaptureObjective(Player& p, double dt) {
    // Head for the nearest point my team doesn't own.
    const CapturePointMode& cm = *capture_;
    size_t best = 0;
    float bestD = std::numeric_limits<float>::max();
    bool foundUnowned = false;
    for (size_t i = 0; i < cm.pointCount(); ++i) {
        float d = p.pos.distance(cm.pointPos(i));
        if (cm.pointOwner(static_cast<int>(i)) == p.team) continue;
        if (!foundUnowned || d < bestD) { foundUnowned = true; bestD = d; best = i; }
    }
    if (!foundUnowned) { // all ours: drift to the nearest point anyway
        for (size_t i = 0; i < cm.pointCount(); ++i) {
            float d = p.pos.distance(cm.pointPos(i));
            if (d < bestD) { bestD = d; best = i; }
        }
    }
    Vec3 tgt = cm.pointPos(best);
    Vec3 to = tgt - p.pos;
    float d = to.length();
    if (d > 1.0f) {
        float stepLen = kPlayerSpeed * static_cast<float>(dt);
        p.pos += (d <= stepLen) ? to : to.normalized() * stepLen;
    }
}

void Match::updateMobaObjective(Player& p, double dt) {
    // Advance down my lane toward the enemy base, fighting minions and
    // towers on the way; the mode handles minion/tower targeting of players.
    MobaDefense& md = *moba_;
    float frac = std::min(1.0f, p.laneFrac + 0.05f * static_cast<float>(dt));
    Vec3 tgt = md.lanePoint(p.team, p.lane, frac);
    Vec3 to = tgt - p.pos;
    float d = to.length();
    float stepLen = kPlayerSpeed * static_cast<float>(dt);
    if (d <= std::max(stepLen, 2.0f)) {
        p.laneFrac = frac; // reached this waypoint: keep marching
    } else {
        p.pos += to.normalized() * stepLen;
    }
    // Hit nearby enemy structures / minions (range 3 melee; structures
    // need the player to walk up to them). Only when no enemy player was
    // in reach this tick (attack priority: players first).
}

void Match::swingAtStructures(Player& p) {
    MobaDefense& md = *moba_;
    if (p.swingCd > 0.0) return;
    float structHp = md.enemyTowerHpNear(p.team, p.pos, kMeleeRange + 1.0f);
    float dmg = kMeleeDps * static_cast<float>(kSwingCooldown);
    md.playerHitMinions(p.team, p.pos, kMeleeRange + 1.0f, dmg);
    md.playerHitStructures(p.team, p.pos, kMeleeRange + 1.0f, dmg, p.id);
    if (structHp > 0.0f) p.swingCd = kSwingCooldown;
}

bool Match::attackNearestEnemy(Player& p, size_t idx, double dt) {
    (void)dt;
    if (p.swingCd > 0.0 || !p.alive) return false;
    size_t best = players_.size();
    float bestD = kMeleeRange;
    for (size_t i = 0; i < players_.size(); ++i) {
        if (i == idx) continue;
        const Player& o = players_[i];
        if (!o.alive || o.team == p.team) continue;
        float d = p.pos.distance(o.pos);
        if (d < bestD) { bestD = d; best = i; }
    }
    if (best >= players_.size()) return false;
    p.swingCd = kSwingCooldown;
    stats_.recordDamage(idx, players_[best].id, kMeleeDps * 1.0f, clock_.now());
    damagePlayer(best, kMeleeDps * 1.0f);
    if (!players_[best].alive) killPlayer(best, static_cast<int>(idx), p.id);
    return true;
}

void Match::damagePlayer(size_t victimIdx, float dmg) {
    Player& v = players_[victimIdx];
    if (!v.alive) return;
    v.hp -= dmg;
    if (v.hp <= 0.0f) { v.hp = 0.0f; v.alive = false; }
}

void Match::killPlayer(size_t victimIdx, int killerIdx, uint64_t killerEntityId) {
    Player& v = players_[victimIdx];
    double respawn = kCaptureRespawnSec;
    if (moba_) {
        // 5 s + 1 s per elapsed minute, capped at 20 s.
        respawn = std::min(kMobaRespawnCapSec,
                           kMobaRespawnBaseSec + elapsed_ / 60.0);
    }
    v.respawnTimer = respawn;
    if (killerIdx >= 0) {
        const Player& k = players_[static_cast<size_t>(killerIdx)];
        stats_.recordKill(static_cast<size_t>(killerIdx), victimIdx,
                          v.id, clock_.now());
        GameEvent e(EventType::PlayerKilled);
        e.sourceId = k.id;
        e.targetId = v.id;
        e.faction = v.team;
        e.pos = v.pos;
        bus_.publish(e);
    } else {
        GameEvent e(EventType::PlayerKilled);
        e.sourceId = killerEntityId; // minion/tower id (not a player id)
        e.targetId = v.id;
        e.faction = v.team;
        e.pos = v.pos;
        bus_.publish(e);
    }
}

void Match::feedCaptureOccupants() {
    for (size_t i = 0; i < capture_->pointCount(); ++i) {
        int c0 = 0, c1 = 0;
        Vec3 pp = capture_->pointPos(static_cast<int>(i));
        float r = capture_->pointRadius(static_cast<int>(i));
        for (const auto& p : players_) {
            if (!p.alive) continue;
            if (p.pos.distance(pp) > r) continue;
            (p.team == 0 ? c0 : c1)++;
        }
        capture_->setOccupants(i, c0, c1);
    }
}

void Match::feedMobaPlayerTargets() {
    playerTargets_.clear();
    for (const auto& p : players_) {
        if (!p.alive) continue;
        MobaDefense::PlayerTarget t;
        t.id = p.id;
        t.team = p.team;
        t.pos = p.pos;
        t.hp = p.hp;
        t.alive = true;
        playerTargets_.push_back(t);
    }
    moba_->setPlayerTargets(playerTargets_);
}

bool Match::movePlayer(uint64_t id, Vec3 pos) {
    for (auto& p : players_)
        if (p.id == id) { p.pos = pos; return true; }
    return false;
}

net::Message Match::modeStateMessage() const {
    net::ModeState s;
    if (capture_) {
        s.mode = "capture";
        s.score0 = capture_->score(0);
        s.score1 = capture_->score(1);
        for (size_t i = 0; i < capture_->pointCount(); ++i) {
            s.pointOwners.push_back(capture_->pointOwner(static_cast<int>(i)));
            s.pointProg0.push_back(capture_->pointProgress(static_cast<int>(i), 0));
            s.pointProg1.push_back(capture_->pointProgress(static_cast<int>(i), 1));
        }
    } else if (moba_) {
        s.mode = "moba";
        s.baseHp0 = moba_->baseHp(0);
        s.baseHp1 = moba_->baseHp(1);
        for (size_t l = 0; l < moba_->laneCount(); ++l)
            for (int team = 0; team < 2; ++team)
                for (int k = 0; k < MobaDefense::TOWERS_PER_TEAM_PER_LANE; ++k)
                    s.towerHps.push_back(moba_->towerHp(team, static_cast<int>(l), k));
        s.minionCount = static_cast<int>(moba_->minionCount());
    } else {
        s.mode = "none";
    }
    return net::encodeModeState(s);
}

void Match::printStatus(std::ostream& os) const {
    os << "match " << modeName_ << " t=" << static_cast<int>(elapsed_)
       << "s players=" << players_.size();
    if (capture_) {
        os << " score " << capture_->score(0) << "-" << capture_->score(1)
           << " (target " << CapturePointMode::TARGET_SCORE << ")\n";
        for (size_t i = 0; i < capture_->pointCount(); ++i)
            os << "  point " << i << " owner=" << capture_->pointOwner(static_cast<int>(i))
               << " prog0=" << capture_->pointProgress(static_cast<int>(i), 0)
               << " prog1=" << capture_->pointProgress(static_cast<int>(i), 1) << "\n";
    } else if (moba_) {
        os << " baseHP " << moba_->baseHp(0) << "/" << moba_->baseHp(1)
           << " minions=" << moba_->minionCount() << "\n";
        for (size_t l = 0; l < moba_->laneCount(); ++l)
            for (int team = 0; team < 2; ++team) {
                os << "  lane " << l << " team " << team << " towers:";
                for (int k = 0; k < MobaDefense::TOWERS_PER_TEAM_PER_LANE; ++k)
                    os << " " << moba_->towerHp(team, static_cast<int>(l), k);
                os << "\n";
            }
    } else {
        os << " (no mode)\n";
    }
    os << "  roster (KDA):\n";
    auto rows = stats_.rows();
    for (size_t i = 0; i < players_.size() && i < rows.size(); ++i)
        os << "    " << players_[i].name << " t" << players_[i].team
           << (players_[i].alive ? "" : " DEAD") << " "
           << rows[i].kills << "/" << rows[i].deaths << "/"
           << rows[i].assists << "\n";
    if (isOver()) os << "  OVER: winner team " << winner() << "\n";
}

} // namespace cultulhu
