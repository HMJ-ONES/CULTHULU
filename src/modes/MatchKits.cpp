// CULT-ULHU wave 40: directional kit combat for 5v5 matches.
// Helpers live here; the Match methods they serve are defined below.

#include "modes/MatchKits.h"
#include "characters/AbilityFx.h"
#include "characters/CharacterPackageLoader.h"
#include "characters/CharacterRegistry.h"
#include "modes/Match.h"
#include "modes/RosterUtils.h"

#include <cmath>
#include <exception>
#include <string>

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

std::string Match::unlockTag(const Player& p,
                             const std::string& slot) const {
    const CharacterDef* kit = kitOf(p);
    std::string name;
    if (kit) {
        if (slot == "Q") name = kit->qAbility.name;
        else if (slot == "F") name = kit->fAbility.name;
        else if (slot == "ULT") name = kit->rAbility.name;
    }
    return name.empty() ? slot : slot + " (" + name + ")";
}

void Match::assignKit(Player& p) {
    // Wave 42: only the 20 playable roster entities fight in matches.
    // NPC packages are minions/ambient life — never match fighters.
    // Domination has no minions at all.
    // (Shuffle reverted — see Match::start note. Round-robin is the
    // balance-tested composition.)
    const std::vector<std::string> all = kits_.list();
    std::vector<std::string> ids;
    ids.reserve(all.size());
    for (const auto& id : all)
        if (!isNpcPackage(id)) ids.push_back(id);
    if (ids.empty()) {
        p.kitId.clear(); // no kits available: plain melee fighter
        return;
    }
    p.kitId = ids[kitAssignCounter_ % ids.size()];
    ++kitAssignCounter_;
}

// Wave 41: ability unlocks per mode.
// Domination: the full kit is unlocked, but the ult starts on its full
// cooldown — 0% charged, no opening ults.
// DOTGOO: you draft your kit as you level. Level 1: one pending pick
// (Q or F). Level 3: a second pick. Level 6: the ult unlocks.
void Match::initAbilityUnlocks(Player& p) {
    const CharacterDef* kit = kitOf(p);
    if (modeName_ == "moba") {
        p.qUnlocked = p.fUnlocked = p.rUnlocked = false;
        p.pendingPicks = 1; // level 1: choose your first ability
        p.qCd = p.fCd = p.rCd = 0.0;
        p.rChargePending = false; // DOTGOO ults arrive via draft, never charged
    } else {
        p.qUnlocked = p.fUnlocked = p.rUnlocked = true;
        p.pendingPicks = 0;
        p.qCd = p.fCd = 0.0;
        // Ult starts at 0%: full cooldown from the opening whistle.
        p.rCd = kit ? kit->rAbility.cooldownSec : 45.0;
        // Micro-dev (2026-10-12): arm the one-shot "ult ready" ping for
        // this opening charge.
        p.rChargePending = true;
    }
    p.rmbCd = 0.0;
}

// Wave 41: spend a pending DOTGOO draft pick.
bool Match::draftAbility(uint64_t id, int slot) {
    for (auto& p : players_) {
        if (p.id != id) continue;
        if (p.pendingPicks <= 0) return false;
        std::string picked;
        if (slot == 0 && !p.qUnlocked) {
            p.qUnlocked = true;
            picked = "Q";
        } else if (slot == 1 && !p.fUnlocked) {
            p.fUnlocked = true;
            picked = "F";
        } else {
            return false;
        }
        --p.pendingPicks;
        // Micro-dev: human draft picks used to be silent on the bus —
        // bot picks published AbilityUnlocked (console + exported match
        // log) but a human's `mpick` never did. Same event, same tag.
        GameEvent e(EventType::AbilityUnlocked);
        e.sourceId = p.id;
        e.pos = p.pos;
        e.tag = unlockTag(p, picked);
        bus_.publish(e);
        return true;
    }
    return false;
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
    // Wave 42: Cthulhu's rock is a frontal shield — the shared helper
    // halves damage from the 120-degree arc he's facing and drains
    // durability in proportion to the blocked damage.
    applyRockShield(victimIdx, atk.pos, final);
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
    // Wave 41: locked abilities can't be cast.
    const bool unlocked =
        (slot == 0) ? p.qUnlocked : (slot == 1) ? p.fUnlocked : p.rUnlocked;
    if (!unlocked) return false;
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
        // Micro-dev (2026-10-12): healing back above half HP re-arms the
        // low-health warning — warn at <35%, re-arm at >=50% (hysteresis
        // so hovering at the line doesn't spam).
        if (p.hp >= p.maxHp * 0.5f) p.lowHpWarned = false;
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
        // Micro-dev (2026-10-15): the cast happens at the caster — stamp
        // pos so the engine's FX pipeline renders the cast visual at the
        // fighter, not the world origin.
        e.pos = p.pos;
        bus_.publish(e);
    }
    return spent;
}

bool Match::castMatchRmb(size_t idx, RmbAim aim) {
    if (idx >= players_.size()) return false;
    Player& p = players_[idx];
    const CharacterDef* kit = kitOf(p);
    if (!kit || !p.alive || p.ccTimer > 0.0) return false;

    // Wave 42: Cthulhu's rock (Domination only — no minions to dominate).
    // RMB aimed down rips the rock; RMB while holding swings it;
    // RMB-launch hurls it. The stun gaze still works without the rock.
    // Swings/throws run on the swing cooldown (the rock is already in
    // hand) — only ripping and gazing cost the RMB cooldown.
    const bool isCthulhu = (p.kitId == "cthulhu_avatar");
    const bool domination = (modeName_ == "capture");
    if (isCthulhu && domination && p.hasRock) {
        if (aim == RmbAim::Launch) return throwRock(idx);
        return swingRock(idx);
    }
    if (p.rmbCd > 0.0) return false;
    if (isCthulhu && domination && aim == RmbAim::Down) return ripRock(idx);
    // else: fall through to the standard heavy attack below

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
        e.pos = p.pos; // heavy attacks land from the fighter's position
        bus_.publish(e);
    }
    return spent;
}

// Wave 42: Cthulhu's rock (Domination). Rip a massive chunk from the
// ground — shield + 3 heavy swings, or hurl it.
bool Match::ripRock(size_t idx) {
    Player& p = players_[idx];
    p.hasRock = true;
    p.rockDurability = 3.0f; // 3 rock-swings of wear, or ~6 blocked melee-hits
    p.rmbCd = 6.0;
    GameEvent e(EventType::AbilityCast);
    e.sourceId = p.id;
    e.tag = kRockRipTag;
    e.amount = 3;
    e.pos = p.pos; // the rip tears out of the ground at the holder
    bus_.publish(e);
    return true;
}

// Wave 42: how much punishment the rock has left, in melee-hit units.
// Shared by swings (1.0 each) and blocked damage (proportional).
static float rockMeleeHitUnit(const Match::Player& p) {
    return Match::kMeleeDps * 2.0f * (1.0f + 0.02f * p.level) * p.dmgMult;
}

void Match::breakRock(size_t idx, const char* why) {
    Player& p = players_[idx];
    p.hasRock = false;
    p.rockDurability = 0.0f;
    GameEvent e(EventType::AbilityCast);
    e.sourceId = p.id;
    e.tag = why;
    e.amount = 3;
    e.pos = p.pos; // the shatter happens at the holder's position
    bus_.publish(e);
}

// Wave 42 fix: the rock's frontal shield, shared by kit damage and
// melee auto-attacks. When the attacker stands in the holder's frontal
// 120° arc, incoming damage is halved and the rock loses durability in
// proportion to the blocked damage: a fresh rock soaks about 6 blocked
// level-appropriate melee hits before crumbling (the wave-42 design
// note). Heavier blocked hits wear it faster; attacks from behind
// ignore it entirely.
void Match::applyRockShield(size_t victimIdx, const Vec3& attackerPos,
                            float& final) {
    Player& v = players_[victimIdx];
    if (!v.hasRock) return;
    if (!inAttackCone(v.pos, v.facingYaw, attackerPos, 1e6f, 60.0f)) return;
    const float blocked = final * 0.5f;
    final = blocked;
    const float unit = rockMeleeHitUnit(v);
    if (unit > 0.0f) v.rockDurability -= (blocked / unit) * 2.0f;
    if (v.rockDurability <= 0.0f) breakRock(victimIdx, "the rock shatters");
}

bool Match::swingRock(size_t idx) {
    Player& p = players_[idx];
    if (!p.hasRock || p.swingCd > 0.0) return false;
    const float base = rockMeleeHitUnit(p);
    bool hit = false;
    for (size_t i = 0; i < players_.size(); ++i) {
        if (i == idx) continue;
        const Player& o = players_[i];
        if (!o.alive || o.team == p.team) continue;
        if (inAttackCone(p.pos, p.facingYaw, o.pos, kMeleeRange * 1.5f,
                         60.0f)) {
            dealMatchDamage(idx, i, base);
            hit = true;
        }
    }
    if (!hit) return false;
    p.swingCd = 1.0;
    p.rockDurability -= 1.0f; // one swing = one melee-hit of wear
    if (p.rockDurability <= 0.0f) {
        breakRock(idx, "the rock crumbles to dust");
    } else {
        // Micro-dev (2026-10-13): a landed swing is Cthulhu's signature
        // Domination attack — publish it so the FX pipeline has a hook
        // (rip/hurl/break all had one; the swing was silent). The
        // breaking swing is covered by breakRock's event instead, so
        // one swing never fires two visuals. MatchLogger deliberately
        // keeps non-landmark RMB casts out of the exported story.
        GameEvent e(EventType::AbilityCast);
        e.sourceId = p.id;
        e.tag = kRockSwingTag;
        e.amount = 3; // slot 3 = RMB
        e.pos = p.pos; // the swing cleaves from the holder's position
        bus_.publish(e);
    }
    return true;
}

bool Match::throwRock(size_t idx) {
    Player& p = players_[idx];
    if (!p.hasRock || p.swingCd > 0.0) return false;
    // Wave 42: a pristine rock hits hardest — launch damage scales with
    // remaining integrity. Swing twice then hurl, or soak hits on the
    // shield then hurl: both are valid before it breaks.
    const float unit = rockMeleeHitUnit(p);
    const float base = unit * (1.0f + 0.5f * p.rockDurability);
    bool hit = false;
    for (size_t i = 0; i < players_.size(); ++i) {
        if (i == idx) continue;
        const Player& o = players_[i];
        if (!o.alive || o.team == p.team) continue;
        if (inAttackCone(p.pos, p.facingYaw, o.pos, kMeleeRange * 4.0f,
                         25.0f)) {
            dealMatchDamage(idx, i, base);
            players_[i].ccTimer = std::max(players_[i].ccTimer, 1.0);
            hit = true;
        }
    }
    if (!hit) return false;
    p.swingCd = 1.0;
    breakRock(idx, kRockHurlTag);
    return true;
}

// Bot brain: face the nearest threat, then spend abilities like a player
// would — ult when it matters, Q/F on cooldown in range, RMB to finish.
void Match::updateBotAbilities(Player& p, size_t idx, double dt) {
    (void)dt;
    if (!p.alive || p.ccTimer > 0.0) return;

    // Wave 41: DOTGOO draft — bots spend pending picks immediately,
    // Q first (damage), then F (utility).
    while (p.pendingPicks > 0) {
        std::string picked;
        if (!p.qUnlocked) {
            p.qUnlocked = true;
            picked = "Q";
        } else if (!p.fUnlocked) {
            p.fUnlocked = true;
            picked = "F";
        } else {
            break; // nothing left to draft
        }
        --p.pendingPicks;
        GameEvent e(EventType::AbilityUnlocked);
        e.sourceId = p.id;
        e.pos = p.pos;
        e.tag = unlockTag(p, picked);
        bus_.publish(e);
    }

    // Fear: drop everything and run from the source.
    if (p.fearTimer > 0.0) {
        p.facingYaw = yawTo(p.fearFrom, p.pos); // face away
        float spd = (p.slowTimer > 0.0) ? kPlayerSpeed * 0.5f : kPlayerSpeed;
        const float stepLen = spd * static_cast<float>(dt);
        p.pos.x += std::cos(p.facingYaw) * stepLen;
        p.pos.z += std::sin(p.facingYaw) * stepLen;
        return;
    }

    // Micro-dev (2026-10-12): self-preservation. A kit-holding bot below
    // 35% HP backs off from the nearest enemy instead of brawling to
    // death — this buys time for heal/shield cooldowns (fired by the
    // self-cast block below) and for teammates to arrive. The bot moves
    // and faces away, which also keeps its directional melee swing silent
    // while fleeing (attackNearestEnemy needs the frontal 60° cone).
    // Kit-less bots are test props that stand their ground. Domination
    // only: the same MOBA-pacing caution as the proactive self-cast block
    // below — DOTGOO bots keep their lane behavior until the owner
    // re-tunes them deliberately. Runs before the self-cast block so a
    // ready heal can't eat the retreat tick; castMatchSpell never touches
    // facingYaw, so the away-facing survives.
    const CharacterDef* botKit = kitOf(p);
    bool retreating = false;
    if (botKit && modeName_ == "capture" && p.hp < p.maxHp * 0.35f) {
        const int threat = enemyInCone(p, idx, 30.0f, 180.0f);
        if (threat >= 0) {
            const Player& foeT = players_[static_cast<size_t>(threat)];
            p.facingYaw = yawTo(foeT.pos, p.pos); // face away
            const float spd =
                (p.slowTimer > 0.0) ? kPlayerSpeed * 0.5f : kPlayerSpeed;
            const float stepLen = spd * static_cast<float>(dt);
            p.pos.x += std::cos(p.facingYaw) * stepLen;
            p.pos.z += std::sin(p.facingYaw) * stepLen;
            retreating = true;
        }
    }

    // Micro-dev (2026-10-12): self-targeted kinds need no enemy. A bot
    // standing around with a summon/shield/buff/heal off cooldown used to
    // hold it forever (the proximity gate below), so summoners never
    // summoned and supports never supported in bot hands. These fire
    // proactively on cooldown; heals wait until the bot is actually hurt.
    // Domination only: in DOTGOO even one proactively-buffed bot cascades
    // through the whole 20-minute pacing (seed-99 guardrail in
    // tests_wave21), so MOBA bot behavior stays exactly as it was until
    // the owner re-tunes it deliberately.
    if (botKit && modeName_ == "capture") {
        for (int slot = 0; slot < 3; ++slot) {
            double& cd = (slot == 0) ? p.qCd : (slot == 1) ? p.fCd : p.rCd;
            if (cd > 0.0) continue;
            const SpellDef* sp = (slot == 0)   ? &botKit->qAbility
                                 : (slot == 1) ? &botKit->fAbility
                                               : &botKit->rAbility;
            const std::string& kind = sp->effectKind;
            const bool selfKind = (kind == "summon" || kind == "buff" ||
                                   kind == "shield" || kind == "heal");
            if (!selfKind) continue;
            if (kind == "heal" && p.hp >= p.maxHp * 0.8f) continue;
            if (castMatchSpell(idx, slot)) return;
        }
    }

    if (retreating) return; // fled this tick: don't turn back to fight

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
    // Wave 42: Cthulhu's rock (Domination) — rip it when brawling 2+,
    // swing it in melee, hurl it at distant foes.
    const bool isCthulhu = (p.kitId == "cthulhu_avatar");
    const bool domination = (modeName_ == "capture");
    if (isCthulhu && domination) {
        if (p.hasRock) {
            // Wave 42: nearly-spent rock? Hurl it before it crumbles —
            // a throw always beats losing it to the next blocked hit.
            if (p.rockDurability < 1.0f && foeDist <= kMeleeRange * 4.0f) {
                if (castMatchRmb(idx, RmbAim::Launch)) return;
            }
            if (foeDist <= kMeleeRange * 1.5f) {
                if (castMatchRmb(idx, RmbAim::Normal)) return; // swing
            } else if (foeDist <= kMeleeRange * 4.0f) {
                if (castMatchRmb(idx, RmbAim::Launch)) return; // hurl
            }
        } else if (p.rmbCd <= 0.0) {
            const int crowd = enemiesInConeCount(p, idx, kMeleeRange * 2.0f,
                                                 60.0f);
            if (crowd >= 2 && foeDist <= kMeleeRange * 2.0f) {
                if (castMatchRmb(idx, RmbAim::Down)) return; // rip the rock
            } else if (foeDist <= kMeleeRange * 2.0f) {
                castMatchRmb(idx, RmbAim::Normal); // stun gaze
            }
        }
    } else if (p.rmbCd <= 0.0 && foeDist <= kMeleeRange * 2.0f) {
        castMatchRmb(idx);
    }
}

// Micro-dev: `mcast` tail-arg parser (see Match.h). Words before numbers.
std::pair<float, Match::RmbAim> Match::parseCastArgs(std::istream& in,
                                                     float defaultYaw) {
    float yaw = defaultYaw;
    RmbAim aim = RmbAim::Normal;
    std::string tok;
    if (in >> tok) {
        if (tok == "down") aim = RmbAim::Down;
        else if (tok == "launch") aim = RmbAim::Launch;
        else {
            try {
                yaw = std::stof(tok) * 3.14159265f / 180.0f;
            } catch (const std::exception&) {
                // unknown token — keep current facing
            }
            // Optional verb after an explicit yaw: `mcast rmb 90 launch`.
            if (in >> tok) {
                if (tok == "down") aim = RmbAim::Down;
                else if (tok == "launch") aim = RmbAim::Launch;
            }
        }
    }
    return {yaw, aim};
}

// Wave 40: human kit casting in matches.
bool Match::humanCast(uint64_t id, int slot, float yaw, RmbAim aim) {
    for (size_t i = 0; i < players_.size(); ++i) {
        if (players_[i].id != id) continue;
        Player& p = players_[i];
        if (!p.alive || p.ccTimer > 0.0) return false;
        p.facingYaw = yaw; // aim the cast
        if (slot == 3) return castMatchRmb(i, aim);
        if (slot >= 0 && slot <= 2) return castMatchSpell(i, slot);
        return false;
    }
    return false;
}

} // namespace cultulhu
