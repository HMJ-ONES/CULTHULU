#include "modes/MobaDefense.h"
#include "core/EventBus.h"
#include "discovery/RelicNames.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cultulhu {

MobaDefense::MobaDefense(EventBus& bus, GameClock& clock, RNG& rng)
    : GameMode(bus, clock), rng_(rng) {
    bases_[0].team = 0;
    bases_[1].team = 1;
}

void MobaDefense::addLane(const std::vector<Vec3>& waypoints) {
    if (waypoints.size() >= 2) lanes_.push_back(waypoints);
}

void MobaDefense::setBase(int team, Vec3 pos, float hp) {
    Base& b = bases_[team];
    b.pos = pos;
    b.hp = b.maxHp = hp;
    b.gooAlive = true;
    b.set = true;
}

MobaDefense::Minion MobaDefense::makeMinion(int team, int lane,
                                           const std::string& kind, Vec3 pos,
                                           float empower) {
    Minion m;
    m.id = nextMinionId_++;
    m.team = team;
    m.lane = lane;
    m.kind = kind;
    m.pos = pos;
    // Wave scaling (standard MOBA inevitability): later waves are tougher.
    // +2% HP/DPS per wave interval of elapsed match time; symmetric, so
    // it amplifies whoever is already winning rather than deciding
    // anything by itself.
    float scale = empower *
        (1.0f + 0.02f * static_cast<float>(elapsed_ / WAVE_INTERVAL));
    if (kind == "ranged") {
        m.hp = m.maxHp = 70.0f * scale; m.dps = 12.0f * scale;
        m.range = 9.0f; m.speed = 3.0f;
    } else if (kind == "siege") {
        m.hp = m.maxHp = 300.0f * scale; m.dps = 30.0f * scale;
        m.range = 6.0f; m.speed = 2.0f;
    } else { // melee
        m.hp = m.maxHp = 120.0f * scale; m.dps = 8.0f * scale;
        m.range = 4.0f; m.speed = 3.0f;
    }
    return m;
}

void MobaDefense::spawnWave(int team, int lane) {
    if (lanes_.empty()) return;
    int l = lane % static_cast<int>(lanes_.size());
    ++waveNumber_;
    bool siegeWave = (waveNumber_ % 3 == 0);
    // Super-minion pressure: cracking the enemy's towers in a lane
    // empowers our waves in that lane (x1.75 HP/DPS) — the snowball that
    // actually ends games. Guarded by towersBuilt_ so towerless test maps
    // don't get phantom empowerment.
    float empower =
        (towersBuilt_ && laneTowersDown(1 - team, l)) ? 1.75f : 1.0f;

    const auto& wps = lanes_[l];
    Vec3 start = (team == 0) ? wps.front() : wps.back();
    auto jitter = [&]() {
        return start + Vec3(rng_.uniform(-2, 2), 0, rng_.uniform(-2, 2));
    };
    // Standard wave: 3 melee + 1 ranged; every 3rd wave adds a siege engine.
    for (int i = 0; i < 3; ++i)
        minions_.push_back(makeMinion(team, l, "melee", jitter(), empower));
    minions_.push_back(makeMinion(team, l, "ranged", jitter(), empower));
    if (siegeWave)
        minions_.push_back(makeMinion(team, l, "siege", jitter(), empower));
}

void MobaDefense::setupDefaultMap() {
    setBase(0, Vec3(-150, 0, 0), GOO_MAX_HP);
    setBase(1, Vec3(150, 0, 0), GOO_MAX_HP);
    // 3 lanes (z=-40, 0, 40); the mid waypoint bows outward for character.
    addLane({Vec3(-150, 0, -40), Vec3(0, 0, -55), Vec3(150, 0, -40)});
    addLane({Vec3(-150, 0, 0), Vec3(0, 0, 55), Vec3(150, 0, 0)});
    addLane({Vec3(-150, 0, 40), Vec3(0, 0, 55), Vec3(150, 0, 40)});
    // Build towers NOW, not lazily on first update: Match updates players
    // before the mode, so lazy towers leave backdoor protection vacuous
    // (and sieging blind) for the opening tick.
    ensureTowers();
    // Wave 33/34: tiered relic pickups in the neutral jungle — the SAME
    // named artifacts as free-roam (generateRelicName pool). Harder
    // tiers sit deeper, need a channel, and the mythic one manifests
    // late with a global announcement.
    relics_.clear();
    auto addRelic = [&](Vec3 pos, int tier, float ampLo, float ampHi,
                        double spawnTime) {
        RelicPickup r;
        r.pos = pos;
        r.tier = tier;
        r.name = generateRelicName(rng_);
        r.amplifier = ampLo + rng_.uniform(0.0f, ampHi - ampLo);
        r.spawnTime = spawnTime;
        relics_.push_back(r);
    };
    // Tier 1: side jungles, from the start, walk-over claim.
    addRelic(Vec3(-75, 0, -20), 1, 0.10f, 0.15f, 0.0);
    addRelic(Vec3(75, 0, 20), 1, 0.10f, 0.15f, 0.0);
    // Tier 2: mid jungle (contested), from the start, 4s channel.
    addRelic(Vec3(0, 0, -27), 2, 0.18f, 0.28f, 0.0);
    addRelic(Vec3(0, 0, 27), 2, 0.18f, 0.28f, 0.0);
    // Tier 3: the center, manifests at 10:00, 6s channel.
    addRelic(Vec3(0, 0, 0), 3, 0.30f, 0.45f, 600.0);
}

void MobaDefense::setPlayerTargets(const std::vector<PlayerTarget>& targets) {
    playerTargets_ = targets;
}

void MobaDefense::setPlayerDamageHook(
    std::function<void(uint64_t, float, uint64_t)> hook) {
    playerDamageHook_ = std::move(hook);
}

int MobaDefense::liveMinionCount(int team) const {
    int n = 0;
    for (const auto& m : minions_)
        if (m.team == team && m.hp > 0.0f) ++n;
    return n;
}

bool MobaDefense::teamTowerAlive(int team) const {
    for (const auto& t : towers_)
        if (t.team == team && t.alive()) return true;
    return false;
}

Vec3 MobaDefense::lanePoint(int team, int lane, float fraction) const {
    if (lanes_.empty()) return Vec3();
    int l = lane % static_cast<int>(lanes_.size());
    float f = (team == 0) ? fraction : 1.0f - fraction;
    return pointAtFraction(l, f);
}

void MobaDefense::playerHitMinions(int attackerTeam, Vec3 pos, float range,
                                   float dmg) {
    (void)playerHitMinionsCredit(attackerTeam, pos, range, dmg);
}

uint64_t MobaDefense::playerHitMinionsCredit(int attackerTeam, Vec3 pos,
                                             float range, float dmg) {
    Minion* best = nullptr;
    float bestD = range;
    for (auto& m : minions_) {
        if (m.team == attackerTeam || m.hp <= 0.0f) continue;
        float d = pos.distance(m.pos);
        if (d < bestD) { bestD = d; best = &m; }
    }
    if (!best) return 0;
    best->hp -= dmg;
    return best->hp <= 0.0f ? best->id : 0;
}

void MobaDefense::playerHitStructures(int attackerTeam, Vec3 pos, float range,
                                      float dmg, uint64_t attackerId) {
    (void)playerHitStructuresCredit(attackerTeam, pos, range, dmg,
                                    attackerId);
}

bool MobaDefense::playerHitStructuresCredit(int attackerTeam, Vec3 pos,
                                            float range, float dmg,
                                            uint64_t attackerId) {
    int enemy = 1 - attackerTeam;
    Tower* best = nullptr;
    float bestD = range;
    for (auto& t : towers_) {
        if (t.team != enemy || !t.alive()) continue;
        float d = pos.distance(t.pos);
        if (d < bestD) { bestD = d; best = &t; }
    }
    if (best) {
        best->hp -= dmg;
        return !best->alive(); // true when this blow destroys it
    }
    // No tower in reach: swing at the enemy base (GOO) if close.
    const Base& b = bases_[enemy];
    if (b.set && pos.distance(b.pos) <= range) damageBase(enemy, dmg, attackerId);
    return false;
}

float MobaDefense::enemyTowerHpNear(int attackerTeam, Vec3 pos,
                                    float range) const {
    int enemy = 1 - attackerTeam;
    float total = 0.0f;
    for (const auto& t : towers_) {
        if (t.team != enemy || !t.alive()) continue;
        if (pos.distance(t.pos) <= range) total += t.hp;
    }
    return total;
}

float MobaDefense::towerHp(int team, int lane, int idx) const {
    int seen = 0;
    for (const auto& t : towers_) {
        if (t.team == team && t.lane == lane) {
            if (seen == idx) return t.hp;
            ++seen;
        }
    }
    return -1.0f;
}

Vec3 MobaDefense::towerPos(int team, int lane, int idx) const {
    int seen = 0;
    for (const auto& t : towers_) {
        if (t.team == team && t.lane == lane) {
            if (seen == idx) return t.pos;
            ++seen;
        }
    }
    return Vec3();
}

Vec3 MobaDefense::pointAtFraction(int lane, float f) const {
    const auto& wps = lanes_[lane];
    float total = 0.0f;
    for (size_t i = 1; i < wps.size(); ++i) total += wps[i - 1].distance(wps[i]);
    float target = total * f;
    for (size_t i = 1; i < wps.size(); ++i) {
        float seg = wps[i - 1].distance(wps[i]);
        if (target <= seg || i == wps.size() - 1) {
            float t = (seg > 1e-6f) ? target / seg : 0.0f;
            if (t > 1.0f) t = 1.0f;
            return wps[i - 1] + (wps[i] - wps[i - 1]) * t;
        }
        target -= seg;
    }
    return wps.back();
}

void MobaDefense::ensureTowers() {
    if (towersBuilt_ || lanes_.empty() || !bases_[0].set || !bases_[1].set)
        return;
    for (size_t l = 0; l < lanes_.size(); ++l) {
        int li = static_cast<int>(l);
        for (int team = 0; team < 2; ++team) {
            // Towers guard each team's own half of the lane.
            float f1 = (team == 0) ? 0.25f : 0.60f;
            float f2 = (team == 0) ? 0.40f : 0.75f;
            for (float f : {f1, f2}) {
                Tower t;
                t.team = team;
                t.lane = li;
                t.pos = pointAtFraction(li, f);
                towers_.push_back(t);
            }
        }
    }
    towersBuilt_ = true;
}

Vec3 MobaDefense::pathTarget(const Minion& m) const {
    const auto& wps = lanes_[m.lane];
    size_t n = wps.size();
    // Team 0 walks 0 -> n-1; team 1 walks n-1 -> 0.
    size_t idx = (m.team == 0) ? (m.step + 1) : (n - 2 - m.step);
    return wps[idx];
}

bool MobaDefense::pathComplete(const Minion& m) const {
    return m.step + 1 >= lanes_[m.lane].size();
}

float MobaDefense::baseHp(int team) const { return bases_[team].hp; }

bool MobaDefense::laneTowersDown(int team, int lane) const {
    for (const auto& t : towers_) {
        if (t.team == team && t.lane == lane && t.alive()) return false;
    }
    return true;
}

bool MobaDefense::laneOpenFor(int team) const {
    // No lanes at all: nothing protects the GOO (matches the old
    // teamTowerAlive()==false semantics for towerless maps).
    if (lanes_.empty()) return true;
    for (size_t l = 0; l < lanes_.size(); ++l) {
        if (laneTowersDown(team, static_cast<int>(l))) return true;
    }
    return false;
}

int MobaDefense::aliveTowerCount(int team) const {
    int n = 0;
    for (const auto& t : towers_)
        if (t.team == team && t.alive()) ++n;
    return n;
}

bool MobaDefense::nearestEnemyTowerPos(int team, Vec3 pos, float range,
                                       Vec3& out) const {
    int enemy = 1 - team;
    bool found = false;
    float bestD = range;
    for (const auto& t : towers_) {
        if (t.team != enemy || !t.alive()) continue;
        float d = pos.distance(t.pos);
        if (d < bestD) { bestD = d; out = t.pos; found = true; }
    }
    return found;
}

float MobaDefense::laneFraction(int team, int lane, Vec3 pos) const {
    if (lanes_.empty()) return 0.0f;
    int l = lane % static_cast<int>(lanes_.size());
    const auto& wps = lanes_[l];
    float total = 0.0f;
    for (size_t i = 1; i < wps.size(); ++i) total += wps[i - 1].distance(wps[i]);
    if (total < 1e-6f) return 0.0f;
    float best = 0.0f, bestD = std::numeric_limits<float>::max(), acc = 0.0f;
    for (size_t i = 1; i < wps.size(); ++i) {
        Vec3 a = wps[i - 1], b = wps[i];
        Vec3 ab = b - a;
        float len = ab.length();
        float t = 0.0f;
        if (len > 1e-6f) {
            Vec3 ap = pos - a;
            t = (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / (len * len);
            t = std::max(0.0f, std::min(1.0f, t));
        }
        float d = pos.distance(a + ab * t);
        if (d < bestD) { bestD = d; best = (acc + len * t) / total; }
        acc += len;
    }
    return (team == 0) ? best : 1.0f - best;
}

void MobaDefense::damageBase(int team, float dmg, uint64_t attackerId) {
    Base& b = bases_[team];
    if (!b.gooAlive) return;
    // Backdoor protection. The Great Old One cannot be damaged until at
    // least one full lane of its team's towers has fallen.
    if (!laneOpenFor(team)) return;
    b.hp -= dmg;
    if (b.hp <= 0.0f) {
        b.hp = 0.0f;
        b.gooAlive = false; // the Great Old One has fallen
        GameEvent e(EventType::Defeated);
        e.faction = team;
        bus_.publish(e);
        // Wave 16: attribute the killing blow for the Godslayer achievement.
        // attackerId is the killing player's id (sourceId = 0 when a minion
        // lands the final blow).
        GameEvent g(EventType::GreatOldOneSlain);
        g.sourceId = attackerId;
        g.faction = team;
        g.pos = b.pos;
        bus_.publish(g);
    }
}

void MobaDefense::update(double dt) {
    float fdt = static_cast<float>(dt);
    ensureTowers();
    elapsed_ += dt;

    // Periodic waves on every lane for both teams. Skipped per team while
    // that team fields more than MINION_CAP_PER_TEAM live minions (light
    // budget: the host's PC simulates the world for everyone).
    waveTimer_ += dt;
    while (waveTimer_ >= WAVE_INTERVAL) {
        waveTimer_ -= WAVE_INTERVAL;
        for (size_t l = 0; l < lanes_.size(); ++l) {
            if (liveMinionCount(0) <= MINION_CAP_PER_TEAM)
                spawnWave(0, static_cast<int>(l));
            if (liveMinionCount(1) <= MINION_CAP_PER_TEAM)
                spawnWave(1, static_cast<int>(l));
        }
    }

    // Towers shoot the nearest enemy in range — minions and players alike.
    // Wave 32: squared distances (no sqrt per check).
    for (size_t ti = 0; ti < towers_.size(); ++ti) {
        auto& t = towers_[ti];
        if (!t.alive()) continue;
        uint64_t towerId = kTowerIdBase + ti;
        Minion* bestMinion = nullptr;
        const PlayerTarget* bestPlayer = nullptr;
        float bestDSq = t.range * t.range;
        for (auto& m : minions_) {
            if (m.team == t.team || m.hp <= 0) continue;
            float dSq = t.pos.distanceSq(m.pos);
            if (dSq < bestDSq) { bestDSq = dSq; bestMinion = &m; bestPlayer = nullptr; }
        }
        for (const auto& p : playerTargets_) {
            if (p.team == t.team || !p.alive) continue;
            float dSq = t.pos.distanceSq(p.pos);
            if (dSq < bestDSq) { bestDSq = dSq; bestMinion = nullptr; bestPlayer = &p; }
        }
        float dmg = t.dps * fdt;
        if (bestMinion) bestMinion->hp -= dmg;
        else if (bestPlayer && playerDamageHook_)
            playerDamageHook_(bestPlayer->id, dmg, towerId);
    }

    // Minions: fight nearest enemy in range (minions, players, towers),
    // else march; hit structures at lane end.
    // Wave 32: squared distances (no sqrt per check). Strictly
    // behavior-preserving — the same targeting decisions, faster.
    for (auto it = minions_.begin(); it != minions_.end();) {
        Minion& m = *it;

        // Nearest enemy minion in range.
        Minion* foe = nullptr;
        const PlayerTarget* foePlayer = nullptr;
        float bestDSq = m.range * m.range;
        for (auto& o : minions_) {
            if (o.team == m.team || o.hp <= 0) continue;
            const float dSq = m.pos.distanceSq(o.pos);
            if (dSq < bestDSq) {
                bestDSq = dSq;
                foe = &o;
                foePlayer = nullptr;
            }
        }
        // Players are fair game too.
        for (const auto& p : playerTargets_) {
            if (p.team == m.team || !p.alive) continue;
            const float dSq = m.pos.distanceSq(p.pos);
            if (dSq < bestDSq) {
                bestDSq = dSq;
                foe = nullptr;
                foePlayer = &p;
            }
        }
        if (foe) {
            foe->hp -= m.dps * fdt;
        } else if (foePlayer && playerDamageHook_) {
            playerDamageHook_(foePlayer->id, m.dps * fdt, m.id);
        } else {
            // Nearest enemy tower in range.
            Tower* tower = nullptr;
            bestDSq = m.range * m.range;
            for (auto& t : towers_) {
                if (t.team == m.team || !t.alive()) continue;
                const float dSq = m.pos.distanceSq(t.pos);
                if (dSq < bestDSq) { bestDSq = dSq; tower = &t; }
            }
            if (tower) {
                tower->hp -= m.dps * fdt;
            } else if (pathComplete(m)) {
                damageBase(1 - m.team, BASE_DPS_TO_STRUCTURE * fdt);
            } else {
                Vec3 tgt = pathTarget(m);
                Vec3 to = tgt - m.pos;
                float dist = to.length();
                float stepLen = m.speed * fdt;
                if (dist <= stepLen || dist < 1.0f) {
                    m.pos = tgt;
                    ++m.step;
                } else {
                    m.pos += to.normalized() * stepLen;
                }
            }
        }

        if (m.hp <= 0.0f) it = minions_.erase(it);
        else ++it;
    }
    // Wave 33/34: relic claims. Tier 1 is a walk-over pickup; tiers 2-3
    // demand a channel (4s / 6s) — leaving the radius cancels it.
    // Tier 3 manifests at 10:00 with a global announcement.
    const float claimRSq = kRelicClaimRadius * kRelicClaimRadius;
    for (auto& r : relics_) {
        if (r.claimedBy != 0) continue;
        if (elapsed_ < r.spawnTime) continue;
        if (!r.announced && r.spawnTime > 0.0) {
            r.announced = true;
            GameEvent ann(EventType::RelicManifested);
            ann.tag = r.name;
            ann.pos = r.pos;
            ann.amount = r.tier;
            bus_.publish(ann);
        }
        // Find a claimant in radius.
        uint64_t claimant = 0;
        for (const auto& p : playerTargets_) {
            if (!p.alive) continue;
            if (p.pos.distanceSq(r.pos) <= claimRSq) {
                claimant = p.id;
                break;
            }
        }
        const double need = r.tier == 1 ? 0.0 : (r.tier == 2 ? 4.0 : 6.0);
        if (need == 0.0) {
            if (claimant != 0) {
                r.claimedBy = claimant;
                GameEvent e(EventType::RelicClaimed);
                e.sourceId = claimant;
                e.amount = r.amplifier;
                e.tag = r.name;
                e.pos = r.pos;
                bus_.publish(e);
            }
            continue;
        }
        // Channeled claim.
        if (claimant != 0 && claimant == r.channelBy) {
            r.channelT += dt;
        } else if (claimant != 0) {
            r.channelBy = claimant;
            r.channelT = dt;
        } else {
            r.channelBy = 0;
            r.channelT = 0.0;
        }
        if (r.channelBy != 0 && r.channelT >= need) {
            r.claimedBy = r.channelBy;
            GameEvent e(EventType::RelicClaimed);
            e.sourceId = r.channelBy;
            e.amount = r.amplifier;
            e.tag = r.name;
            e.pos = r.pos;
            bus_.publish(e);
            r.channelBy = 0;
            r.channelT = 0.0;
        }
    }
    // Last: match lifecycle sees this tick's final state (a GOO falling
    // inside this update ends the match immediately).
    noteMatchTick("moba");
}

bool MobaDefense::isOver() const {
    return !bases_[0].gooAlive || !bases_[1].gooAlive ||
           elapsed_ >= TIME_LIMIT;
}

int MobaDefense::winner() const {
    if (!bases_[0].gooAlive && !bases_[1].gooAlive) return -1; // draw
    if (!bases_[1].gooAlive) return 0;
    if (!bases_[0].gooAlive) return 1;
    if (elapsed_ >= TIME_LIMIT) {
        // Higher surviving GOO-HP fraction wins; then most towers left;
        // then a draw. Guarantees every match terminates.
        float f0 = bases_[0].hp / GOO_MAX_HP;
        float f1 = bases_[1].hp / GOO_MAX_HP;
        if (std::fabs(f0 - f1) > 1e-6f) return (f0 > f1) ? 0 : 1;
        int t0 = aliveTowerCount(0), t1 = aliveTowerCount(1);
        if (t0 != t1) return (t0 > t1) ? 0 : 1;
        return -1;
    }
    return -1;
}

} // namespace cultulhu
