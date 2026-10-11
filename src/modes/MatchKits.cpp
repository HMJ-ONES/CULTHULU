// CULT-ULHU wave 40: directional kit combat for 5v5 matches.
// Helpers live here; the Match methods they serve are defined below.

#include "modes/MatchKits.h"
#include "characters/CharacterPackageLoader.h"
#include "characters/CharacterRegistry.h"
#include "modes/Match.h"

#include <cmath>

namespace cultulhu {

float yawTo(const Vec3& from, const Vec3& to) {
    return std::atan2(to.z - from.z, to.x - from.x);
}

bool inAttackCone(const Vec3& casterPos, float facingYaw, const Vec3& targetPos,
                  float range, float halfAngleDeg) {
    const float dx = targetPos.x - casterPos.x;
    const float dz = targetPos.z - casterPos.z;
    const float distSq = dx * dx + dz * dz;
    if (distSq > range * range) return false;
    if (distSq < 1e-6f) return true; // on top of the caster: always hit
    const float targetYaw = std::atan2(dz, dx);
    float diff = targetYaw - facingYaw;
    while (diff > M_PI) diff -= 2.0f * static_cast<float>(M_PI);
    while (diff < -M_PI) diff += 2.0f * static_cast<float>(M_PI);
    const float halfRad = halfAngleDeg * static_cast<float>(M_PI) / 180.0f;
    return std::fabs(diff) <= halfRad;
}

int loadMatchKits(CharacterRegistry& reg, const std::string& base) {
    CharacterPackageLoader loader{reg};
    // The driver runs from the repo root (assets/characters); the test
    // binaries run from build/ (../assets/characters). Try both.
    const std::vector<std::string> roots = {base + "/characters",
                                            base + "/../assets/characters",
                                            "./assets/characters"};
    for (const auto& r : roots) {
        const int n = loader.scanAndLoad(r);
        if (n > 0) return n;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Match kit-combat methods.
// ---------------------------------------------------------------------------

const CharacterDef* Match::kitOf(const Player& p) const {
    if (p.kitId.empty()) return nullptr;
    return kits_.get(p.kitId);
}

void Match::assignKit(Player& p) {
    const std::vector<std::string> ids = kits_.list();
    if (ids.empty()) {
        p.kitId.clear(); // no kits available: plain melee fighter
        return;
    }
    p.kitId = ids[kitAssignCounter_ % ids.size()];
    ++kitAssignCounter_;
}

// Match-scale damage: kit powers are authored for free-roam (500-HP
// entities); match fighters have 100 HP, so scale to ~1/5. Levels add a
// gentle ramp so late-game casts hit harder without one-shotting.
float Match::kitDamage(const Player& p, float effectPower) const {
    return effectPower * kKitDmgScale * (1.0f + 0.02f * p.level) * p.dmgMult;
}

// Nearest living enemy player inside the frontal cone.
int Match::enemyInCone(const Player& p, size_t idx, float range,
                       float halfAngleDeg) const {
    int best = -1;
    float bestD = range;
    for (size_t i = 0; i < players_.size(); ++i) {
        if (i == idx) continue;
        const Player& o = players_[i];
        if (!o.alive || o.team == p.team) continue;
        const float d = p.pos.distance(o.pos);
        if (d < bestD &&
            inAttackCone(p.pos, p.facingYaw, o.pos, range, halfAngleDeg)) {
            bestD = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int Match::enemiesInConeCount(const Player& p, size_t idx, float range,
                              float halfAngleDeg) const {
    int n = 0;
    for (size_t i = 0; i < players_.size(); ++i) {
        if (i == idx) continue;
        const Player& o = players_[i];
        if (!o.alive || o.team == p.team) continue;
        if (inAttackCone(p.pos, p.facingYaw, o.pos, range, halfAngleDeg)) ++n;
    }
    return n;
}

// One damage application: attacker's buff, victim's shield, stats, death.
void Match::dealMatchDamage(size_t attackerIdx, size_t victimIdx, float dmg) {
    if (attackerIdx >= players_.size() || victimIdx >= players_.size()) return;
    Player& atk = players_[attackerIdx];
    Player& v = players_[victimIdx];
    if (!atk.alive || !v.alive) return;
    float final = (atk.buffTimer > 0.0) ? dmg * 1.3f : dmg;
    if (v.shieldTimer > 0.0) final *= 0.5f;
    v.hp -= final;
    if (v.hp <= 0.0f) {
        v.hp = 0.0f;
        v.alive = false;
    }
    stats_.recordDamage(attackerIdx, v.id, final, clock_.now());
    if (!v.alive) killPlayer(victimIdx, static_cast<int>(attackerIdx), atk.id);
}

bool Match::castMatchSpell(size_t idx, int slot) {
    if (idx >= players_.size()) return false;
    Player& p = players_[idx];
    const CharacterDef* kit = kitOf(p);
    if (!kit || !p.alive || p.ccTimer > 0.0) return false;
    const SpellDef* sp = (slot == 0) ? &kit->qAbility
                         : (slot == 1) ? &kit->fAbility
                                       : &kit->rAbility;
    double& cd = (slot == 0) ? p.qCd : (slot == 1) ? p.fCd : p.rCd;
    if (cd > 0.0) return false;

    const std::string& kind = sp->effectKind;
    const float range = (sp->range > 0.0f) ? sp->range : 8.0f;
    const float base = kitDamage(p, sp->effectPower);
    bool spent = false;

    if (kind == "projectile") {
        // Narrow bolt along the facing: nearest enemy in a 25° half-cone.
        const int t = enemyInCone(p, idx, range, 25.0f);
        if (t >= 0) {
            dealMatchDamage(idx, static_cast<size_t>(t), base);
            spent = true;
        }
    } else if (kind == "aoe_damage") {
        // Wide frontal sweep: everyone in a 45° half-cone takes the hit.
        for (size_t i = 0; i < players_.size(); ++i) {
            if (i == idx) continue;
            const Player& o = players_[i];
            if (!o.alive || o.team == p.team) continue;
            if (inAttackCone(p.pos, p.facingYaw, o.pos, range, 45.0f)) {
                dealMatchDamage(idx, i, base);
                spent = true;
            }
        }
    } else if (kind == "stun") {
        const int t = enemyInCone(p, idx, range, 30.0f);
        if (t >= 0) {
            dealMatchDamage(idx, static_cast<size_t>(t), base * 0.5f);
            players_[t].ccTimer = sp->effectPower; // held fast
            spent = true;
        }
    } else if (kind == "debuff") {
        const int t = enemyInCone(p, idx, range, 30.0f);
        if (t >= 0) {
            dealMatchDamage(idx, static_cast<size_t>(t), base * 0.7f);
            players_[t].slowTimer = 5.0; // withered: half move speed
            spent = true;
        }
    } else if (kind == "fear_aura") {
        // Radial: every enemy in range breaks and flees.
        for (size_t i = 0; i < players_.size(); ++i) {
            if (i == idx) continue;
            Player& o = players_[i];
            if (!o.alive || o.team == p.team) continue;
            if (p.pos.distance(o.pos) <= range) {
                o.fearTimer = sp->effectPower;
                o.fearFrom = p.pos;
                spent = true;
            }
        }
    } else if (kind == "pull") {
        // Frontal cone: victims are dragged toward the caster.
        for (size_t i = 0; i < players_.size(); ++i) {
            if (i == idx) continue;
            Player& o = players_[i];
            if (!o.alive || o.team == p.team) continue;
            if (inAttackCone(p.pos, p.facingYaw, o.pos, range, 35.0f)) {
                Vec3 to = p.pos - o.pos;
                const float d = to.length();
                if (d > 1e-3f) o.pos += to.normalized() * (d * 0.5f);
                dealMatchDamage(idx, i, base * 0.4f);
                spent = true;
            }
        }
    } else if (kind == "dash") {
        // Lunge along the facing — engage or escape.
        const float dist = std::min(sp->effectPower, 15.0f);
        p.pos.x += std::cos(p.facingYaw) * dist;
        p.pos.z += std::sin(p.facingYaw) * dist;
        spent = true;
    } else if (kind == "buff") {
        p.buffTimer = sp->effectPower; // +30% damage while active
        spent = true;
    } else if (kind == "shield") {
        p.shieldTimer = sp->effectPower; // half damage taken while active
        spent = true;
    } else if (kind == "heal") {
        p.hp = std::min(p.maxHp, p.hp + base * 2.0f);
        spent = true;
    } else if (kind == "summon") {
        // Call servants: in DOTGOO they spawn as real allied minions that
        // fight for 30s; in Domination (no lanes) the surge becomes a
        // frontal-cone mauling instead.
        const int n = std::max(1, static_cast<int>(sp->effectPower));
        if (moba_) {
            moba_->spawnSummon(p.team, p.pos, p.lane, n, 30.0);
        } else {
            for (size_t i = 0; i < players_.size(); ++i) {
                if (i == idx) continue;
                const Player& o = players_[i];
                if (!o.alive || o.team == p.team) continue;
                if (inAttackCone(p.pos, p.facingYaw, o.pos, range, 45.0f)) {
                    dealMatchDamage(idx, i, base);
                    spent = true;
                }
            }
        }
        spent = true;
    }

    if (spent) {
        cd = (sp->cooldownSec > 0.0f) ? sp->cooldownSec : 8.0f;
        GameEvent e(EventType::AbilityCast);
        e.sourceId = p.id;
        e.tag = sp->name;
        e.amount = slot;
        bus_.publish(e);
    }
    return spent;
}

bool Match::castMatchRmb(size_t idx) {
    if (idx >= players_.size()) return false;
    Player& p = players_[idx];
    const CharacterDef* kit = kitOf(p);
    if (!kit || !p.alive || p.ccTimer > 0.0 || p.rmbCd > 0.0) return false;
    const HeavyAttackDef& hk = kit->rightClick;
    const float range = (hk.range > 0.0f) ? hk.range : kMeleeRange * 1.5f;
    const float base =
        kMeleeDps * hk.damageMult * (1.0f + 0.02f * p.level) * p.dmgMult;
    bool spent = false;

    switch (hk.kind) {
        case HeavyAttackKind::MeleeHeavy: {
            // Big frontal cleave: wide cone, heavy damage.
            for (size_t i = 0; i < players_.size(); ++i) {
                if (i == idx) continue;
                const Player& o = players_[i];
                if (!o.alive || o.team == p.team) continue;
                if (inAttackCone(p.pos, p.facingYaw, o.pos, range, 50.0f)) {
                    dealMatchDamage(idx, i, base * 2.0f);
                    spent = true;
                }
            }
            break;
        }
        case HeavyAttackKind::MindControl: {
            // Dominating gaze: narrow cone, light damage, brief stun.
            const int t = enemyInCone(p, idx, range + 4.0f, 25.0f);
            if (t >= 0) {
                dealMatchDamage(idx, static_cast<size_t>(t), base * 0.5f);
                players_[t].ccTimer = 1.5;
                spent = true;
            }
            break;
        }
        case HeavyAttackKind::AcidSpit: {
            // Ranged cone of caustic spit.
            for (size_t i = 0; i < players_.size(); ++i) {
                if (i == idx) continue;
                const Player& o = players_[i];
                if (!o.alive || o.team == p.team) continue;
                if (inAttackCone(p.pos, p.facingYaw, o.pos, range + 4.0f,
                                 30.0f)) {
                    dealMatchDamage(idx, i, base * 1.2f);
                    spent = true;
                }
            }
            break;
        }
        case HeavyAttackKind::EldritchGrasp: {
            // Grasping cone: drag victims in and root them briefly.
            for (size_t i = 0; i < players_.size(); ++i) {
                if (i == idx) continue;
                Player& o = players_[i];
                if (!o.alive || o.team == p.team) continue;
                if (inAttackCone(p.pos, p.facingYaw, o.pos, range + 3.0f,
                                 35.0f)) {
                    Vec3 to = p.pos - o.pos;
                    const float d = to.length();
                    if (d > 1e-3f) o.pos += to.normalized() * (d * 0.4f);
                    dealMatchDamage(idx, i, base * 0.8f);
                    o.ccTimer = std::max(o.ccTimer, 1.0);
                    spent = true;
                }
            }
            break;
        }
    }

    if (spent) {
        p.rmbCd = 6.0; // heavy attacks are deliberate, not spammable
        GameEvent e(EventType::AbilityCast);
        e.sourceId = p.id;
        e.tag = hk.name;
        e.amount = 3; // slot 3 = RMB
        bus_.publish(e);
    }
    return spent;
}

// Bot brain: face the nearest threat, then spend abilities like a player
// would — ult when it matters, Q/F on cooldown in range, RMB to finish.
void Match::updateBotAbilities(Player& p, size_t idx, double dt) {
    (void)dt;
    if (!p.alive || p.ccTimer > 0.0) return;

    // Fear: drop everything and run from the source.
    if (p.fearTimer > 0.0) {
        p.facingYaw = yawTo(p.fearFrom, p.pos); // face away
        float spd = (p.slowTimer > 0.0) ? kPlayerSpeed * 0.5f : kPlayerSpeed;
        const float stepLen = spd * static_cast<float>(dt);
        p.pos.x += std::cos(p.facingYaw) * stepLen;
        p.pos.z += std::sin(p.facingYaw) * stepLen;
        return;
    }

    // Face the nearest enemy — all attacks are directional now.
    int nearest = enemyInCone(p, idx, 40.0f, 180.0f); // 180° = any direction
    if (nearest < 0) return; // nobody near: hold for the objective logic
    Player& foe = players_[static_cast<size_t>(nearest)];
    p.facingYaw = yawTo(p.pos, foe.pos);

    const float foeDist = p.pos.distance(foe.pos);
    const CharacterDef* kit = kitOf(p);
    const float qRange = kit ? std::max(kit->qAbility.range, 4.0f) : 6.0f;

    // R (ult): when it can hit 2+, or the duel is serious.
    if (p.rCd <= 0.0 && kit && foeDist <= kit->rAbility.range + 2.0f) {
        const int crowd = enemiesInConeCount(p, idx, kit->rAbility.range, 45.0f);
        if (crowd >= 2 || foe.hp > 60.0f) {
            if (castMatchSpell(idx, 2)) return;
        }
    }
    // Q/F: spend on cooldown whenever something is in reach.
    if (p.qCd <= 0.0 && foeDist <= qRange + 1.0f) {
        if (castMatchSpell(idx, 0)) return;
    }
    if (p.fCd <= 0.0 && kit && foeDist <= kit->fAbility.range + 1.0f) {
        if (castMatchSpell(idx, 1)) return;
    }
    // RMB: heavy finisher at close range.
    if (p.rmbCd <= 0.0 && foeDist <= kMeleeRange * 2.0f) {
        castMatchRmb(idx);
    }
}

// Wave 40: human kit casting in matches.
bool Match::humanCast(uint64_t id, int slot, float yaw) {
    for (size_t i = 0; i < players_.size(); ++i) {
        if (players_[i].id != id) continue;
        Player& p = players_[i];
        if (!p.alive || p.ccTimer > 0.0) return false;
        p.facingYaw = yaw; // aim the cast
        if (slot == 3) return castMatchRmb(i);
        if (slot >= 0 && slot <= 2) return castMatchSpell(i, slot);
        return false;
    }
    return false;
}

} // namespace cultulhu
