#include "modes/Match.h"
#include "discovery/RelicNames.h"
#include "modes/MatchKits.h"
#include "modes/RosterUtils.h"

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
    // Wave 33: MOBA relic claims — the holder's damage multiplier grows.
    // Guarded to moba mode: free-roam publishes RelicClaimed too (avatar
    // claims), and those must not touch match players.
    // 8 PM bug-fix (2026-10-12): the handle is stored so ~Match() can
    // unsubscribe — otherwise this [this]-capturing handler survives the
    // Match on the shared session bus (use-after-free on the next match).
    relicSub_ = bus_.subscribe(EventType::RelicClaimed, [this](const GameEvent& e) {
        if (!moba_ || modeName_ != "moba") return;
        for (auto& p : players_) {
            if (p.id != e.sourceId) continue;
            p.relicNames.push_back(e.tag);
            p.dmgMult = std::min(3.0f, p.dmgMult + e.amount);
            break;
        }
    });
}

Match::~Match() {
    // 8 PM bug-fix (2026-10-12): the constructor subscribed on the session
    // bus, which outlives every Match — drop the handler here.
    bus_.unsubscribe(relicSub_);
}

bool Match::start(const std::string& mode) {
    modeName_ = mode;
    players_.clear();
    stats_.clear();
    elapsed_ = 0.0;
    firstBloodDone_ = false; // micro-dev (2026-10-12): arena callout resets
    nextBotNum_ = 1;
    kitAssignCounter_ = 0;
    // Wave 40: every fighter wields a full character kit (Q/F/R + RMB).
    // Kits are the same data as free-roam; when they fail to load (e.g. a
    // bare test env), fighters fall back to plain directional melee.
    if (kits_.count() == 0) loadMatchKits(kits_, "assets");
    // NOTE (wave 42): kit shuffle reverted — random compositions can
    // stalemate the MOBA (see tests_wave21 seed 99). Composition balance
    // needs design work before replayability via shuffle. The shuffledKits_
    // deck stays for future use.
    mode_.reset();
    capture_ = nullptr;
    moba_ = nullptr;
    if (mode == "capture") {
        auto m = std::make_unique<CapturePointMode>(bus_, clock_);
        m->setupDominationPoint();
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
    assignKit(p); // wave 40: humans wield kits too
    initAbilityUnlocks(p); // wave 41
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
    assignKit(p); // wave 40: every fighter wields a character kit
    initAbilityUnlocks(p); // wave 41: Domination/DOTGOO unlock rules
    spawnPlayer(p);
    players_.push_back(p);
    stats_.setPlayerName(players_.size() - 1, p.name);
    return p.id;
}

uint64_t Match::joinAsHuman(int team) {
    if (team < 0) team = (countTeam(0) <= countTeam(1)) ? 0 : 1;
    team = (team == 0) ? 0 : 1;
    for (size_t i = 0; i < players_.size(); ++i) {
        Player& p = players_[i];
        if (p.isBot && p.team == team) {
            p.isBot = false;
            p.name = "You";
            p.swingCd = 0.0;
            p.facingYaw = 0.0f;
            spawnPlayer(p); // fresh spawn at the team base, full HP
            stats_.setPlayerName(i, p.name);
            return p.id;
        }
    }
    return 0;
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
    // Micro-dev (2026-10-12): a fresh (re)spawn re-arms the low-health
    // warning — the next 35%-crossing warns again.
    p.lowHpWarned = false;
    p.respawnTimer = 0.0;
    p.swingCd = 0.0;
    p.laneFrac = 0.0f;
    p.hasRock = false; // wave 42: the rock doesn't survive death
    p.rockDurability = 0.0f;
}

const Match::Player* Match::findPlayer(uint64_t id) const {
    for (const auto& p : players_)
        if (p.id == id) return &p;
    return nullptr;
}

void Match::update(double dt) {
    if (!mode_) return;
    if (isOver()) return; // freeze the final state for scoreboard/status
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
    // Wave 40: kit cooldowns and status timers tick for everyone.
    if (p.qCd > 0) p.qCd -= dt;
    if (p.fCd > 0) p.fCd -= dt;
    // Micro-dev (2026-10-12): the Domination opening ult charge is the one
    // cooldown a player actually waits on — ping once when it completes so
    // nobody has to poll `match status`. One-shot: the flag is consumed on
    // fire, so casting the ult and re-charging it stays silent.
    const bool rWasCharging = p.rCd > 0.0;
    if (p.rCd > 0) p.rCd -= dt;
    if (p.rChargePending && rWasCharging && p.rCd <= 0.0) {
        p.rChargePending = false;
        GameEvent uc(EventType::UltCharged);
        uc.sourceId = p.id;
        const CharacterDef* kit = kitOf(p);
        uc.tag = kit ? kit->rAbility.name : "ult";
        uc.faction = p.team;
        bus_.publish(uc);
    }
    if (p.rmbCd > 0) p.rmbCd -= dt;
    if (p.ccTimer > 0) p.ccTimer -= dt;
    if (p.fearTimer > 0) p.fearTimer -= dt;
    if (p.buffTimer > 0) p.buffTimer -= dt;
    if (p.shieldTimer > 0) p.shieldTimer -= dt;
    if (p.slowTimer > 0) p.slowTimer -= dt;
    if (!p.alive) {
        p.respawnTimer -= dt;
        p.invoking = false;
        p.invokeTimer = 0.0;
        if (p.respawnTimer <= 0.0) {
            // Micro-dev (2026-10-12): a dead player comes back silently
            // today — the respawn countdown row just clears. Publish so
            // the driver can announce the return.
            GameEvent rp(EventType::PlayerRespawned);
            rp.sourceId = p.id;
            rp.tag = p.name;
            rp.faction = p.team;
            bus_.publish(rp);
            spawnPlayer(p);
        }
        return;
    }
    // Stunned fighters cannot act at all — no movement, no abilities.
    if (p.ccTimer > 0.0) return;
    // Wave 33/34: base invocation. A player with invoke charges AND
    // level 10+ standing at their base channels for 20s; leaving or
    // dying cancels. Completion calls down a free-roam artifact of the
    // highest tier the player's level unlocks (10/20/30) — even one
    // never found in the jungle.
    if (moba_ && p.pendingInvokes > 0 &&
        p.level >= MobaDefense::kTier1Level) {
        const Vec3 base = moba_->basePos(p.team);
        if (p.pos.distance(base) <= kBaseInvokeRadius) {
            if (!p.invoking) {
                p.invoking = true;
                p.invokeTimer = 0.0;
            }
            p.invokeTimer += dt;
            if (p.invokeTimer >= kInvokeChannelSec) {
                p.invoking = false;
                p.invokeTimer = 0.0;
                --p.pendingInvokes;
                const int tier = p.level >= MobaDefense::kTier3Level ? 3
                                 : p.level >= MobaDefense::kTier2Level ? 2
                                                                      : 1;
                GameEvent e(EventType::RelicClaimed);
                e.sourceId = p.id;
                // Tier amplifiers mirror the jungle tiers.
                e.amount = tier == 3 ? 0.30f + rng_.uniform(0.0f, 0.15f)
                           : tier == 2 ? 0.18f + rng_.uniform(0.0f, 0.10f)
                                       : 0.10f + rng_.uniform(0.0f, 0.05f);
                e.tag = generateRelicName(rng_);
                e.pos = p.pos;
                e.faction = p.team;
                bus_.publish(e);
            }
        } else {
            p.invoking = false;
            p.invokeTimer = 0.0;
        }
    }
    // An enemy in melee reach pins the player: stand and fight instead of
    // walking off toward the objective.
    if (!enemyInMeleeRange(p, idx)) {
        // Humans are driven by the driver (`move`); they hold position
        // otherwise. AFK humans get a bot pilot (wave 42). Everyone still
        // auto-attacks via attackNearestEnemy.
        if (p.isBot || p.afk) {
            if (capture_)
                updateCaptureObjective(p, dt);
            else if (moba_)
                updateMobaObjective(p, dt);
        }
    }
    // Enemy players first; only then swing at moba structures/minions.
    // Wave 40: bots fight with their full kit — directional Q/F/R + RMB —
    // before falling back to the directional melee swing.
    // Wave 42: AFK pilots use the kit too.
    if (p.isBot || p.afk) updateBotAbilities(p, idx, dt);
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

// Wave 40: effective move speed — debuffs (slow) halve it.
static float moveSpeedOf(const Match::Player& p) {
    return (p.slowTimer > 0.0) ? Match::kPlayerSpeed * 0.5f
                               : Match::kPlayerSpeed;
}

void Match::updateCaptureObjective(Player& p, double dt) {
    // Domination: everyone converges on the single central point; the
    // melee-pinning in updatePlayer handles the brawl once they arrive.
    const CapturePointMode& cm = *capture_;
    Vec3 tgt = cm.pointPos(0);
    Vec3 to = tgt - p.pos;
    float d = to.length();
    if (d > 1.0f) {
        float stepLen = moveSpeedOf(p) * static_cast<float>(dt);
        Vec3 step = (d <= stepLen) ? to : to.normalized() * stepLen;
        p.pos += step;
        p.facingYaw = std::atan2(step.z, step.x);
    }
}

bool Match::furthestFriendlyMinion(int team, int lane, Vec3& out) const {
    if (!moba_) return false;
    const auto& mins = moba_->minions();
    Vec3 base = moba_->basePos(team);
    bool found = false;
    float bestD = 0.0f;
    for (const auto& m : mins) {
        if (m.team != team || m.lane != lane || m.hp <= 0.0f) continue;
        float d = m.pos.distance(base);
        if (!found || d > bestD) { found = true; bestD = d; out = m.pos; }
    }
    return found;
}

void Match::updateMobaObjective(Player& p, double dt) {
    MobaDefense& md = *moba_;
    float fdt = static_cast<float>(dt);
    // Wave 33/34: a bot holding invoke charges AND level 10+ returns to
    // base to channel them all. (The channel itself runs in updatePlayer.)
    // 8 PM bug-fix (2026-10-12): threshold is 1, not 2 — a bot banking
    // exactly one charge that stops leveling used to sit on it forever.
    // The 180s lastInvokeTrip gate already prevents yo-yoing, so >= 2 was
    // redundant.
    if (p.pendingInvokes >= 1 && p.level >= MobaDefense::kTier1Level &&
        !p.invoking && (elapsed_ - p.lastInvokeTrip) >= 180.0) {
        p.lastInvokeTrip = elapsed_;
        const Vec3 base = md.basePos(p.team);
        Vec3 to = base - p.pos;
        float d = to.length();
        if (d > kBaseInvokeRadius * 0.5f) {
            float stepLen = moveSpeedOf(p) * fdt;
            Vec3 step = (d <= stepLen) ? to : to.normalized() * stepLen;
            p.pos += step;
            p.facingYaw = std::atan2(step.z, step.x);
        }
        return;
    }
    // Push with the minion wave: escort it so the wave tanks the tower,
    Vec3 tp;
    if (md.nearestEnemyTowerPos(p.team, p.pos, 12.0f, tp)) {
        Vec3 to = tp - p.pos;
        float d = to.length();
        if (d > 3.5f) {
            float stepLen = moveSpeedOf(p) * fdt;
            Vec3 step = (d <= stepLen) ? to : to.normalized() * stepLen;
            // Stop at swing range; overshooting walks into the tower.
            if (step.length() > d - 3.0f && d > 3.0f)
                step = to.normalized() * (d - 3.0f);
            if (step.length() > 0.01f) {
                p.pos += step;
                p.facingYaw = std::atan2(step.z, step.x);
            }
        }
        return;
    }
    float cur = md.laneFraction(p.team, p.lane, p.pos);
    Vec3 anchor;
    Vec3 tgt;
    if (furthestFriendlyMinion(p.team, p.lane, anchor) &&
        md.laneFraction(p.team, p.lane, anchor) > cur + 0.02f) {
        Vec3 baseDir = md.basePos(p.team) - anchor;
        float bl = baseDir.length();
        tgt = (bl > 1e-3f) ? anchor + baseDir.normalized() * 2.0f : anchor;
    } else {
        // No wave ahead: march down the lane from where we actually are.
        float frac = std::min(1.0f, cur + 0.05f * fdt);
        tgt = md.lanePoint(p.team, p.lane, frac);
    }
    Vec3 to = tgt - p.pos;
    float d = to.length();
    if (d > 1.0f) {
        float stepLen = moveSpeedOf(p) * fdt;
        Vec3 step = (d <= stepLen) ? to : to.normalized() * stepLen;
        p.pos += step;
        p.facingYaw = std::atan2(step.z, step.x);
    }
}

void Match::swingAtStructures(Player& p) {
    MobaDefense& md = *moba_;
    if (p.swingCd > 0.0) return;
    float structHp = md.enemyTowerHpNear(p.team, p.pos, kMeleeRange + 1.0f);
    // Wave 33: relic damage multiplier applies to structures and waveclear.
    float dmg = kMeleeDps * static_cast<float>(kSwingCooldown) * p.dmgMult;
    float clearDmg =
        kWaveclearDps * static_cast<float>(kSwingCooldown) * p.dmgMult;
    // Wave 34: minion kills grant 60 XP; tower destructions grant 800 XP.
    size_t idx = 0;
    for (; idx < players_.size(); ++idx)
        if (players_[idx].id == p.id) break;
    if (md.playerHitMinionsCredit(p.team, p.pos, kMeleeRange + 1.0f,
                                  clearDmg) != 0 &&
        idx < players_.size())
        addXp(idx, 60);
    if (md.playerHitStructuresCredit(p.team, p.pos, kMeleeRange + 1.0f, dmg,
                                     p.id) &&
        idx < players_.size())
        addXp(idx, 800);
    // Wave 34 fix: the swing always costs its cooldown, even when only
    // minions were in reach (previously minions could be machine-gunned
    // every tick — harmless before XP existed, an exploit now).
    p.swingCd = kSwingCooldown;
}

bool Match::attackNearestEnemy(Player& p, size_t idx, double dt) {
    (void)dt;
    if (p.swingCd > 0.0 || !p.alive || p.ccTimer > 0.0) return false;
    size_t best = players_.size();
    float bestD = kMeleeRange;
    for (size_t i = 0; i < players_.size(); ++i) {
        if (i == idx) continue;
        const Player& o = players_[i];
        if (!o.alive || o.team == p.team) continue;
        float d = p.pos.distance(o.pos);
        // Wave 40: melee is directional — a frontal 120° cone. Standing
        // behind someone is safe; you must face your target to hit them.
        if (d < bestD &&
            inAttackCone(p.pos, p.facingYaw, o.pos, kMeleeRange, 60.0f)) {
            bestD = d;
            best = i;
        }
    }
    if (best >= players_.size()) return false;
    p.swingCd = kSwingCooldown;
    // Wave 33: relic damage multiplier. Wave 40: buff timer.
    float dmg = kMeleeDps * p.dmgMult;
    if (p.buffTimer > 0.0) dmg *= 1.3f;
    stats_.recordDamage(idx, players_[best].id, dmg, clock_.now());
    // Wave 42 fix: pass the attacker's position so the rock's frontal
    // shield can block melee auto-attacks (it used to bypass the shield).
    damagePlayer(best, dmg, players_[idx].pos);
    if (!players_[best].alive) killPlayer(best, static_cast<int>(idx), p.id);
    return true;
}

void Match::damagePlayer(size_t victimIdx, float dmg) {
    Player& v = players_[victimIdx];
    if (!v.alive) return;
    // Wave 40: shield halves incoming damage while active.
    const float final = (v.shieldTimer > 0.0) ? dmg * 0.5f : dmg;
    v.hp -= final;
    if (v.hp <= 0.0f) { v.hp = 0.0f; v.alive = false; }
    else checkLowHp(victimIdx);
}

// Micro-dev (2026-10-12): one-shot low-health warning per 35%-crossing.
// Bots already retreat at this line on their own; the driver announces
// the event for the joined human. The killing blow stays silent — death
// has its own announcement.
void Match::checkLowHp(size_t victimIdx) {
    Player& v = players_[victimIdx];
    if (!v.alive || v.lowHpWarned || v.maxHp <= 0.0f) return;
    if (v.hp / v.maxHp >= 0.35f) return;
    v.lowHpWarned = true;
    GameEvent e(EventType::PlayerLowHp);
    e.sourceId = v.id;
    e.faction = v.team;
    e.amount = v.hp / v.maxHp;
    e.tag = v.name;
    e.pos = v.pos;
    bus_.publish(e);
}

// Micro-dev (2026-10-12): test-only — raw damage through the production
// path. A lethal hit mirrors the combat paths: killPlayer runs so the
// respawn timer and death announcements behave like a real kill.
bool Match::debugDamagePlayer(uint64_t id, float dmg) {
    for (size_t i = 0; i < players_.size(); ++i) {
        if (players_[i].id != id) continue;
        const bool wasAlive = players_[i].alive;
        damagePlayer(i, dmg);
        if (wasAlive && !players_[i].alive) killPlayer(i, -1, 0);
        return true;
    }
    return false;
}

// Wave 42 fix: melee auto-attacks route through here so Cthulhu's rock
// can block them — the old path had no attacker position, so the
// frontal shield silently did nothing against the most common damage
// source in Domination.
void Match::damagePlayer(size_t victimIdx, float dmg,
                         const Vec3& attackerPos) {
    Player& v = players_[victimIdx];
    if (!v.alive) return;
    // Wave 40: shield halves incoming damage while active.
    float final = (v.shieldTimer > 0.0) ? dmg * 0.5f : dmg;
    applyRockShield(victimIdx, attackerPos, final);
    v.hp -= final;
    if (v.hp <= 0.0f) { v.hp = 0.0f; v.alive = false; }
    else checkLowHp(victimIdx);
}

// Wave 34: XP economy. Level = 1 + xp/50; each level-up grants one
// invoke charge.
void Match::debugSetPendingInvokes(size_t playerIdx, int n) {
    if (playerIdx < players_.size()) players_[playerIdx].pendingInvokes = n;
}

void Match::addXp(size_t playerIdx, int amount) {
    if (playerIdx >= players_.size() || amount <= 0) return;
    Player& p = players_[playerIdx];
    const int oldLevel = p.level;
    p.xp += amount;
    // Wave 40: level caps at 30 (the tier-3 invocation threshold). Beyond
    // that, XP still banks invoke charges but the level — and the damage
    // scaling derived from it — stays bounded.
    p.level = std::min(30, 1 + p.xp / 50);
    if (p.level > oldLevel) {
        p.pendingInvokes += (p.level - oldLevel);
        // 2 PM build (2026-10-12): the level loop was narratively silent —
        // one event per addXp call even on multi-level jumps, so the
        // invocation-tier gates (10/20/30) are visible to the player.
        GameEvent le(EventType::PlayerLeveledUp);
        le.sourceId = p.id;
        le.amount = static_cast<float>(p.level);
        le.tag = p.name;
        bus_.publish(le);
        // Wave 41: DOTGOO ability draft. Level 3: second pick (Q/F).
        // Level 6: the ult (R) unlocks.
        if (modeName_ == "moba") {
            for (int lv = oldLevel + 1; lv <= p.level; ++lv) {
                // Wave 41: DOTGOO ability draft. Level 3: second pick (Q/F).
                // Wave 42 fix: a banked level-1 pick used to eat the
                // level-3 pick (the old `pendingPicks == 0` guard skipped
                // the grant, amputating Q or F for the whole match).
                // Picks accumulate now: bank one, cross 3, hold two.
                if (lv == 3 && !(p.qUnlocked && p.fUnlocked))
                    p.pendingPicks += 1;
                if (lv == 6 && !p.rUnlocked) {
                    p.rUnlocked = true;
                    GameEvent e(EventType::AbilityUnlocked);
                    e.sourceId = p.id;
                    e.tag = unlockTag(p, "ULT");
                    bus_.publish(e);
                }
            }
        }
    }
}

void Match::killPlayer(size_t victimIdx, int killerIdx, uint64_t killerEntityId) {
    Player& v = players_[victimIdx];
    v.killStreak = 0; // wave 42: death ends the streak
    // Micro-dev (2026-10-12): first blood — the match's opening death gets
    // an arena callout, whatever (or whoever) lands it. Once per match;
    // PlayerKilled right below still carries every death.
    if (!firstBloodDone_) {
        firstBloodDone_ = true;
        const bool byPlayer = killerIdx >= 0;
        GameEvent fb(EventType::FirstBlood);
        fb.sourceId = byPlayer
            ? players_[static_cast<size_t>(killerIdx)].id
            : killerEntityId;
        fb.targetId = v.id;
        fb.faction = byPlayer
            ? players_[static_cast<size_t>(killerIdx)].team
            : -1;
        bus_.publish(fb);
    }
    double respawn = kCaptureRespawnSec;
    if (moba_) {
        // 5 s + 1 s per elapsed minute, capped at 20 s.
        respawn = std::min(kMobaRespawnCapSec,
                           kMobaRespawnBaseSec + elapsed_ / 60.0);
    }
    v.respawnTimer = respawn;
    if (killerIdx >= 0) {
        Player& k = players_[static_cast<size_t>(killerIdx)];
        stats_.recordKill(static_cast<size_t>(killerIdx), victimIdx,
                          v.id, clock_.now());
        // Wave 34: kill XP scales with the victim's level — early kills
        // on level-1 players are worth little; slaying a level-20 is
        // worth a fortune. (Prevents 3 early kills = level 10.)
        addXp(static_cast<size_t>(killerIdx), 50 + 25 * v.level);
        // Wave 42: kill streaks — 8s window, announce at 2+.
        if (elapsed_ - k.lastKillAt < 8.0)
            ++k.killStreak;
        else
            k.killStreak = 1;
        k.lastKillAt = elapsed_;
        if (k.killStreak >= 2) {
            GameEvent se(EventType::KillStreak);
            se.sourceId = k.id;
            se.amount = k.killStreak;
            se.tag = k.name;
            bus_.publish(se);
        }
        GameEvent e(EventType::PlayerKilled);
        e.sourceId = k.id;
        e.targetId = v.id;
        e.faction = v.team;
        e.pos = v.pos;
        bus_.publish(e);
        // Domination scores player kills (DOTGOO's default hook ignores it).
        if (mode_) mode_->notePlayerKill(k.team);
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

// Test/GM utility: grant XP directly (tests stage level-ups).
bool Match::grantXp(uint64_t id, int amount) {
    for (size_t i = 0; i < players_.size(); ++i)
        if (players_[i].id == id) { addXp(i, amount); return true; }
    return false;
}

// Wave 40: assign a specific kit (tests stage deterministic fights).
bool Match::setPlayerKit(uint64_t id, const std::string& kitId) {
    for (auto& p : players_)
        if (p.id == id) {
            p.kitId = kitId;
            p.qCd = p.fCd = p.rmbCd = 0.0;
            if (modeName_ == "moba") {
                // DOTGOO: ults arrive via draft, never charged — keep the
                // zeroed cooldowns and leave draft unlocks untouched.
                p.rCd = 0.0;
                p.rChargePending = false;
            } else {
                // 8 PM bug-fix (2026-10-12): a kit swap is NOT a free ult.
                // Domination's rule is the ult starts at 0% charge with its
                // full R cooldown from the opening whistle
                // (initAbilityUnlocks) — re-apply it here instead of
                // zeroing, so `match join <kit-id>` can't cheat one in.
                // Q/F stay ready; draft unlocks untouched.
                const CharacterDef* kit = kitOf(p);
                p.rCd = kit ? kit->rAbility.cooldownSec : 45.0;
                p.rChargePending = true; // ready-ping fires on completion
            }
            return true;
        }
    return false;
}

// Wave 42: AFK takeover. Bot pilots the fighter; match never pauses.
// afkSeconds accumulates for the end-of-match rewards call.
void Match::setPlayerAfk(uint64_t id, bool afk) {
    for (auto& p : players_) {
        if (p.id != id) continue;
        if (p.afk == afk) return;
        p.afk = afk;
        if (afk) {
            p.afkSince = elapsed_;
        } else {
            p.afkSeconds += elapsed_ - p.afkSince;
        }
        return;
    }
}

bool Match::isPlayerAfk(uint64_t id) const {
    for (const auto& p : players_)
        if (p.id == id) return p.afk;
    return false;
}

// Wave 42: AFK fraction for rewards. If the player is currently AFK,
// include the ongoing stretch.
double Match::afkFraction(uint64_t id) const {
    for (const auto& p : players_) {
        if (p.id != id) continue;
        double afk = p.afkSeconds;
        if (p.afk) afk += elapsed_ - p.afkSince;
        if (elapsed_ <= 0.0) return 0.0;
        return afk / elapsed_;
    }
    return 0.0;
}

bool Match::setFacingYaw(uint64_t id, float yaw) {
    for (auto& p : players_)
        if (p.id == id) { p.facingYaw = yaw; return true; }
    return false;
}

net::Message Match::modeStateMessage() const {
    net::ModeState s;
    if (capture_) {
        s.mode = "capture";
        s.score0 = capture_->score(0);
        s.score1 = capture_->score(1);
        // Domination: single point; owner slot carries the current holder
        // (-1 = open/contested), progress slots unused (no capture bar).
        s.pointOwners.push_back(capture_->holder());
        s.pointProg0.push_back(0.0f);
        s.pointProg1.push_back(0.0f);
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
    printStatus(os, 0);
}

void Match::printStatus(std::ostream& os, uint64_t humanId) const {
    // Wave 39: display names — Domination and DOTGOO.
    std::string displayMode = modeName_;
    if (modeName_ == "capture") displayMode = "domination";
    if (modeName_ == "moba") displayMode = "DOTGOO";
    os << "match " << displayMode << " t=" << static_cast<int>(elapsed_)
       << "s players=" << players_.size();
    if (capture_) {
        os << " score " << capture_->score(0) << "-" << capture_->score(1)
           << " (target " << CapturePointMode::TARGET_SCORE << ")\n";
        os << "  point: ";
        if (capture_->overtime())
            os << "OVERTIME " << static_cast<int>(capture_->overtimeElapsed()) << "s";
        else if (capture_->contested())
            os << "contested (ticking paused)";
        else if (capture_->holder() >= 0)
            os << "team " << capture_->holder() << " holding";
        else
            os << "open";
        os << " | t=" << static_cast<int>(capture_->elapsed()) << "s"
           << " (limit " << static_cast<int>(CapturePointMode::TIME_LIMIT) << "s)\n";
    } else if (moba_) {
        os << " baseHP " << moba_->baseHp(0) << "/" << moba_->baseHp(1)
           << " minions=" << moba_->minionCount()
           // Micro-dev: minion waves march every WAVE_INTERVAL seconds but
           // the status header never showed the countdown — a player
           // timing a lane push couldn't see the next wave coming. Print
           // it from the same timer that spawns the waves (ceil, so it
           // never reads 0s while a wave is still inbound).
           << " wave in " << static_cast<int>(moba_->nextWaveIn() + 0.999)
           << "s"
           // Micro-dev: Domination's status line shows its time limit — the
           // DOTGOO line didn't, so a player couldn't see how close the
           // 20:00 whistle was. Show it the same way, sourced from the
           // same constant that ends the match.
           << " (limit " << static_cast<int>(MobaDefense::TIME_LIMIT) << "s)\n";
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
    for (size_t i = 0; i < players_.size() && i < rows.size(); ++i) {
        const Player& pl = players_[i];
        // Micro-dev: mark the local human's row (" (YOU)") so they can find
        // their own fighter in a 10-row bot roster at a glance.
        os << "    " << pl.name;
        if (humanId != 0 && pl.id == humanId) os << " (YOU)";
        os << " t" << pl.team;
        if (pl.alive) {
            os << " ";
        } else {
            // Micro-dev: dead players respawn on a timer — show the countdown
            // in the row ("DEAD (respawn 4s)") so the wait is visible at a
            // glance without typing anything else.
            os << " DEAD (respawn "
               << static_cast<int>(pl.respawnTimer + 0.999) << "s) ";
        }
        os << rows[i].kills << "/" << rows[i].deaths << "/"
           << rows[i].assists;
        // Convenience pass: HP at a glance — matches deal damage, so the
        // bar belongs in the roster row.
        os << " " << static_cast<int>(pl.hp) << "/"
           << static_cast<int>(pl.maxHp) << "hp";
        // Wave 33/34: level, relics, invoke state.
        os << " lv" << pl.level << " (" << pl.xp << "xp)";
        // Wave 40: kit identity — every fighter wields a character kit.
        if (!pl.kitId.empty()) {
            const CharacterDef* kd = kitOf(pl);
            os << " <" << (kd ? kd->displayName : pl.kitId) << ">";
        }
        // Convenience pass: kit cooldowns at a glance, like free-roam
        // `status`. Unlocked slot: "ready" or Ns remaining; locked: "-".
        // (Wave 41 showed only unlock brackets — you couldn't tell if the
        // ult was 5s or 40s away.) Micro-dev: RMB is the fourth button
        // and runs its own cooldown (6s heavy attack, 7s gaze, rock
        // rip/swing) — it was invisible here until now. RMB has no
        // unlock gating (unlike Q/F/R in DOTGOO), so it always shows.
        os << " [";
        {
            const char* keys[4] = {"Q", "F", "R", "RMB"};
            const bool unlocked[4] = {pl.qUnlocked, pl.fUnlocked,
                                      pl.rUnlocked, true};
            const double cds[4] = {pl.qCd, pl.fCd, pl.rCd, pl.rmbCd};
            for (int k = 0; k < 4; ++k) {
                if (k) os << " ";
                os << keys[k];
                if (!unlocked[k]) os << "-";
                else if (cds[k] > 0.0)
                    os << static_cast<int>(cds[k] + 0.5) << "s";
                else os << " ready";
            }
        }
        os << "]";
        // Wave 42: rock durability at a glance (3.0 on rip, breaks at 0).
        if (pl.hasRock)
            os << " rock " << pl.rockDurability << "/3.0";
        if (pl.pendingPicks > 0) os << " (+" << pl.pendingPicks << " pick)";
        if (!pl.relicNames.empty()) {
            os << " [";
            for (size_t r = 0; r < pl.relicNames.size(); ++r) {
                if (r) os << ", ";
                os << pl.relicNames[r];
            }
            os << " x" << pl.dmgMult << "]";
        }
        if (pl.invoking)
            os << " (invoking " << static_cast<int>(pl.invokeTimer) << "/20s)";
        else if (pl.pendingInvokes > 0 && pl.level >= MobaDefense::kTier1Level)
            os << " (" << pl.pendingInvokes << " invoke ready — return to base)";
        os << "\n";
    }
    if (moba_) {
        os << "  relics on the map:\n";
        for (const auto& r : moba_->relics()) {
            os << "    [t" << r.tier << "] " << r.name << " (+" 
               << static_cast<int>(r.amplifier * 100) << "%) at ("
               << static_cast<int>(r.pos.x) << "," << static_cast<int>(r.pos.z)
               << ")";
            if (r.claimedBy != 0)
                os << " — claimed";
            else if (r.spawnTime > 0.0)
                os << " — manifests at " << static_cast<int>(r.spawnTime) << "s";
            else if (r.tier > 1)
                os << " — channel " << (r.tier == 2 ? "4" : "6") << "s to claim";
            os << "\n";
        }
    }
    if (isOver()) os << "  OVER: winner team " << winner() << "\n";
}

void Match::printMatchSurvey(std::ostream& os, uint64_t humanId) const {
    // Micro-dev (2026-10-12): see Match.h. Direction words mirror the
    // free-roam arrival survey (x+ = east, z+ = south).
    const Player* hp = findPlayer(humanId);
    if (!hp) return;
    if (capture_ && capture_->pointCount() > 0) {
        const Vec3 pp = capture_->pointPos(0);
        const float d = hp->pos.distance(pp);
        const float r = capture_->pointRadius(0);
        os << "  the point: ";
        if (d <= r) {
            os << "YOU ARE ON IT — ";
            const int h = capture_->holder();
            if (h == hp->team) os << "team " << h << " banking\n";
            else if (capture_->contested()) os << "contested, ticking paused\n";
            else if (h >= 0) os << "team " << h << " holding\n";
            else os << "open\n";
        } else {
            os << static_cast<int>(d + 0.5f) << "m "
               << (pp.x >= hp->pos.x ? "east" : "west") << "-"
               << (pp.z >= hp->pos.z ? "south" : "north") << " — ";
            const int h = capture_->holder();
            if (h >= 0) os << "team " << h << " holding\n";
            else if (capture_->contested()) os << "contested\n";
            else os << "open\n";
        }
    }
    const Player* near = nullptr;
    float nearD = 1e9f;
    for (const auto& p : players_) {
        if (p.id == humanId || p.team == hp->team || !p.alive) continue;
        const float d = hp->pos.distance(p.pos);
        if (d < nearD) { nearD = d; near = &p; }
    }
    if (near) {
        os << "  nearest enemy: " << near->name << " ("
           << static_cast<int>(nearD + 0.5f) << "m "
           << (near->pos.x >= hp->pos.x ? "east" : "west") << "-"
           << (near->pos.z >= hp->pos.z ? "south" : "north") << ")\n";
    } else {
        os << "  no living enemies\n";
    }
}

} // namespace cultulhu
