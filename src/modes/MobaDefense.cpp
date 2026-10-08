#include "modes/MobaDefense.h"
#include "core/EventBus.h"

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
                                           const std::string& kind, Vec3 pos) {
    Minion m;
    m.id = nextMinionId_++;
    m.team = team;
    m.lane = lane;
    m.kind = kind;
    m.pos = pos;
    if (kind == "ranged") {
        m.hp = m.maxHp = 70.0f; m.dps = 12.0f; m.range = 9.0f; m.speed = 3.0f;
    } else if (kind == "siege") {
        m.hp = m.maxHp = 300.0f; m.dps = 30.0f; m.range = 6.0f; m.speed = 2.0f;
    } else { // melee
        m.hp = m.maxHp = 120.0f; m.dps = 8.0f; m.range = 4.0f; m.speed = 3.0f;
    }
    return m;
}

void MobaDefense::spawnWave(int team, int lane) {
    if (lanes_.empty()) return;
    int l = lane % static_cast<int>(lanes_.size());
    ++waveNumber_;
    bool siegeWave = (waveNumber_ % 3 == 0);

    const auto& wps = lanes_[l];
    Vec3 start = (team == 0) ? wps.front() : wps.back();
    auto jitter = [&]() {
        return start + Vec3(rng_.uniform(-2, 2), 0, rng_.uniform(-2, 2));
    };
    // Standard wave: 3 melee + 1 ranged; every 3rd wave adds a siege engine.
    for (int i = 0; i < 3; ++i) minions_.push_back(makeMinion(team, l, "melee", jitter()));
    minions_.push_back(makeMinion(team, l, "ranged", jitter()));
    if (siegeWave) minions_.push_back(makeMinion(team, l, "siege", jitter()));
}

float MobaDefense::baseHp(int team) const { return bases_[team].hp; }

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

void MobaDefense::damageBase(int team, float dmg, uint64_t attackerId) {
    Base& b = bases_[team];
    if (!b.gooAlive) return;
    b.hp -= dmg;
    if (b.hp <= 0.0f) {
        b.hp = 0.0f;
        b.gooAlive = false; // the Great Old One has fallen
        GameEvent e(EventType::Defeated);
        e.faction = team;
        bus_.publish(e);
        // Wave 16: attribute the killing blow for the Godslayer achievement.
        GameEvent g(EventType::GreatOldOneSlain);
        g.sourceId = attackerId;
        g.faction = team;
        g.pos = b.pos;
        bus_.publish(g);
    }
}

void MobaDefense::update(double dt) {
    noteMatchTick("moba");
    float fdt = static_cast<float>(dt);
    ensureTowers();

    // Periodic waves on every lane for both teams.
    waveTimer_ += dt;
    while (waveTimer_ >= WAVE_INTERVAL) {
        waveTimer_ -= WAVE_INTERVAL;
        for (size_t l = 0; l < lanes_.size(); ++l) {
            spawnWave(0, static_cast<int>(l));
            spawnWave(1, static_cast<int>(l));
        }
    }

    // Towers shoot the nearest enemy minion in range.
    for (auto& t : towers_) {
        if (!t.alive()) continue;
        Minion* best = nullptr;
        float bestD = t.range;
        for (auto& m : minions_) {
            if (m.team == t.team || m.hp <= 0) continue;
            float d = t.pos.distance(m.pos);
            if (d < bestD) { bestD = d; best = &m; }
        }
        if (best) best->hp -= t.dps * fdt;
    }

    // Minions: fight enemies in range, else march; hit structures at lane end.
    for (auto it = minions_.begin(); it != minions_.end();) {
        Minion& m = *it;

        // Nearest enemy minion in range.
        Minion* foe = nullptr;
        float bestD = m.range;
        for (auto& o : minions_) {
            if (o.team == m.team || o.hp <= 0) continue;
            float d = m.pos.distance(o.pos);
            if (d < bestD) { bestD = d; foe = &o; }
        }
        if (foe) {
            foe->hp -= m.dps * fdt;
        } else {
            // Nearest enemy tower in range.
            Tower* tower = nullptr;
            bestD = m.range;
            for (auto& t : towers_) {
                if (t.team == m.team || !t.alive()) continue;
                float d = m.pos.distance(t.pos);
                if (d < bestD) { bestD = d; tower = &t; }
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
}

bool MobaDefense::isOver() const {
    return !bases_[0].gooAlive || !bases_[1].gooAlive;
}

int MobaDefense::winner() const {
    if (!bases_[0].gooAlive && !bases_[1].gooAlive) return -1; // draw
    if (!bases_[1].gooAlive) return 0;
    if (!bases_[0].gooAlive) return 1;
    return -1;
}

} // namespace cultulhu
