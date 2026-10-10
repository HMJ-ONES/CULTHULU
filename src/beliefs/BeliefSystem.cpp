#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"
#include "core/GameClock.h"

#include <algorithm>

namespace cultulhu {

// ---- tuning (all creative-liberty numbers; documented in README) ----
namespace {
constexpr float TORTURE_PER_VICTIM      = 6.4f;   // civilians/creatures tortured
                                                     // (wave 9a sim: 8.0 made Torture the runaway
                                                     // fastest single belief; -20%)
constexpr float TORTURE_PER_CAPTURED    = 5.0f;   // captured cultists tortured
constexpr float TORTURE_ONE_HIT_KILL   = -12.0f;  // own cultist one-hit killed

constexpr float FEAR_RAID_POWER        = 14.4f;  // per raid * destruction(0..1)
                                                     // (wave 9a sim: 12.0 left steady raiding
                                                     // stalled below 500 power; +20%)
constexpr float FEAR_RAID_LEVEL        = 20.0f;
constexpr float FEAR_DECAY_PER_SEC     = 1.5f;   // when not in combat
constexpr float FEAR_POWER_DECAY       = 1.0f;   // power lost per sec while decaying
constexpr float FEAR_CULTIST_LOST      = -6.0f;
constexpr float FEAR_DEFEATED          = -60.0f;

constexpr float BREED_SUCCESS          = 15.0f;
constexpr float BREED_FERAL_KILL       = -10.0f;

constexpr float CHAOS_EXPLOSION         = 6.0f;
constexpr float CHAOS_IMPRISONED        = -4.0f;
constexpr float CHAOS_ADOPT_SPEED      = 2.0f;   // faster belief adoption
constexpr float PUNISH_RISK_UP         = 10.0f;  // insurrection risk per punishment

constexpr float SACRIFICE_COMPLETED    = 30.0f;  // (wave 9a sim: 25.0 left Sacrifice
                                                     // stalled at ~495 power; +20%)
constexpr float SACRIFICE_INTERRUPTED  = -15.0f;
constexpr float DEATH_DENIED           = 20.0f;  // converted denies a death

constexpr float CONVERSION_PER_SOUL    = 4.0f;

constexpr float WAR_SINGLE_KILL        = 10.0f;
constexpr float WAR_MULTI_RATE         = 0.4f;   // lower rate vs multiple deities
constexpr float WAR_BUILDING_DAMAGED   = -5.0f;
constexpr float WAR_BUILDING_DESTROYED = -20.0f;

constexpr float TRICKERY_MIMIC_KILL    = 8.0f;
constexpr float TRICKERY_MIMIC_SLAIN   = -8.0f;
constexpr float TRICKERY_TRAP_SPRUNG   = 6.0f;
constexpr float TRICKERY_TRAP_FEAR     = 5.0f;

constexpr float MAGIC_NECROMANCY       = 10.0f;

// Wave 9b: new directives.
constexpr float ASSASSIN_FEAR_LEVEL    = 40.0f;  // fear level from a slain leader
constexpr float ASSASSIN_FEAR_POWER    = 30.0f;  // power spike on assassination
constexpr float ASSASSIN_WAR_POWER     = 20.0f;  // war power on assassination
constexpr float BLIGHT_TICK_FEAR       = 1.5f;   // fear per blight tick
constexpr float BLIGHT_TICK_POWER      = 0.5f;   // power per blight tick
constexpr float BLIGHT_DONE_FEAR       = 20.0f;
constexpr float BLIGHT_DONE_POWER      = 10.0f;
constexpr float CHAMPION_MAGIC_POWER   = 25.0f;  // power on successful summoning
constexpr float CHAMPION_FEAR_LEVEL    = 15.0f;  // fear from the summoned horror
constexpr float SUMMON_INTERRUPT_MAGIC = -15.0f; // power lost on interruption

constexpr float ONSLAUGHT_PER_CIVILIAN = 3.6f;   // (wave 9a sim: 3.0 left the kill-fantasy
                                                     // belief far behind War; +20%)
constexpr float ONSLAUGHT_IDLE_DECAY   = 2.0f;   // power lost per sec after 1h idle

constexpr float DREAM_WHISPER_POWER    = 13.0f;  // per dream-whisper conversion
                                                     // (wave 10 / R5: 2.0 left Dreams at 238
                                                     // power; whispers are its only income)
constexpr float RECONSTRUCTION_PER_REBUILD = 2.0f;  // power per rebuilt
                                                     // building (wave 10 / R5:
                                                     // Reconstruction had no
                                                     // power path at all)
constexpr float RECONSTRUCTION_PER_HEAL    = 0.5f;  // power per heal performed

constexpr float PRAYER_POWER           = 1.0f;   // per prayer offered (ambient)
constexpr float DESECRATE_FEAR         = 2.0f;   // fear per desecration
constexpr float DESECRATE_TORTURE      = 2.0f;   // power per desecration (Torture)

// Wave 9c: new ambient activities.
constexpr float OMEN_READ_POWER        = 2.0f;   // power per omen-reading
constexpr float SIGIL_FEAR             = 3.0f;   // fear per painted sigil
constexpr float SPARRING_POWER         = 0.5f;   // power per sparring bout
constexpr float CHANT_POWER            = 0.5f;   // power per chanting circle
// Wave 15: five new ambient activities.
constexpr float DREAMSHARE_POWER       = 1.0f;   // power per shared dream (Dreams)
constexpr float EFFIGY_POWER           = 1.0f;   // power per mended effigy (Reconstruction)
constexpr float RITE_FEAR              = 2.0f;   // fear per blood rite (Fear)
constexpr float HUNT_POWER             = 0.5f;   // power per hunted meat (Onslaught)
} // namespace

BeliefSystem::BeliefSystem(EventBus& bus, GameClock& clock)
    : bus_(bus), clock_(clock) {}

bool BeliefSystem::isActive(Belief b) const {
    return std::find(active_.begin(), active_.end(), b) != active_.end();
}

bool BeliefSystem::requestChange(Belief in, Belief out) {
    if (in == out || in == Belief::Count) return false;
    if (isActive(in)) return false;
    for (const auto& p : pending_)
        if (p.belief == in) return false;

    auto it = std::find(active_.begin(), active_.end(), out);
    if (it != active_.end()) {
        active_.erase(it);
        GameEvent e(EventType::BeliefAdopted);
        e.tag = std::string("removed:") + beliefName(out);
        bus_.publish(e);
    } else if (static_cast<int>(active_.size()) >= MAX_ACTIVE) {
        return false; // no room: must name a belief to remove
    }

    pending_.push_back(Pending{in, ADOPTION_TIME});
    return true;
}

// Save/load: wholesale belief restore (no adoption timer, no events).
void BeliefSystem::restoreActive(const std::vector<Belief>& beliefs) {
    active_.clear();
    pending_.clear();
    for (Belief b : beliefs) {
        if (b == Belief::Count) continue;
        if (static_cast<int>(active_.size()) >= MAX_ACTIVE) break;
        if (isActive(b)) continue;
        active_.push_back(b);
    }
    fear_ = 0.0f;
}

void BeliefSystem::punishInfringer() {    for (auto& p : pending_) p.remaining *= 0.5; // faster adoption
    GameEvent e(EventType::InsurrectionRiskUp);
    e.amount = PUNISH_RISK_UP;
    e.tag = "punish_infringer";
    bus_.publish(e);
    GameEvent e2(EventType::InfringerPunished);
    bus_.publish(e2);
}

// Wave 4: external fear injection (belief synergies, e.g. Dread Broods).
void BeliefSystem::addFear(float amount) {
    fear_ += amount;
    if (fear_ > 100.0f) fear_ = 100.0f;
    if (fear_ < 0.0f) fear_ = 0.0f;
}

void BeliefSystem::update(double dt) {
    double speed = isActive(Belief::Chaos) ? CHAOS_ADOPT_SPEED : 1.0;
    for (auto it = pending_.begin(); it != pending_.end();) {
        it->remaining -= dt * speed;
        if (it->remaining <= 0.0) {
            adoptPending(*it);
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }
}

void BeliefSystem::adoptPending(const Pending& p) {
    if (static_cast<int>(active_.size()) < MAX_ACTIVE && !isActive(p.belief)) {
        active_.push_back(p.belief);
        GameEvent e(EventType::BeliefAdopted);
        e.tag = beliefName(p.belief);
        bus_.publish(e);
    }
}

double BeliefSystem::adoptionRemaining(Belief b) const {
    for (const auto& p : pending_)
        if (p.belief == b) return p.remaining;
    return -1.0;
}

float BeliefSystem::onEvent(const GameEvent& e) {
    float delta = 0.0f;

    if (isActive(Belief::Torture)) {
        switch (e.type) {
            case EventType::TorturePerformed:
                delta += TORTURE_PER_VICTIM * e.amount; break;
            case EventType::CapturedTortured:
                delta += TORTURE_PER_CAPTURED * e.amount; break;
            case EventType::OwnCultistTortured:
                break; // no benefit, no extra insurrection risk (per doc)
            case EventType::CultistOneHitKilled:
                delta += TORTURE_ONE_HIT_KILL; break;
            default: break;
        }
    }

    if (isActive(Belief::Fear)) {
        switch (e.type) {
            case EventType::RaidPerformed:
                fear_ += FEAR_RAID_LEVEL * e.amount *
                         (night_ ? 1.25f : 1.0f); // night raids terrify
                if (fear_ > 100.0f) fear_ = 100.0f;
                delta += FEAR_RAID_POWER * e.amount;
                break;
            case EventType::CapturedInArmy:
                fearPassiveGen_ += 2.0f * e.amount;
                break;
            case EventType::CultistLost:
                fear_ -= 6.0f; if (fear_ < 0.0f) fear_ = 0.0f;
                delta += FEAR_CULTIST_LOST;
                break;
            case EventType::Defeated:
                fear_ = 0.0f;
                delta += FEAR_DEFEATED;
                break;
            // Wave 9b: a slain enemy leader terrifies the populace.
            case EventType::LeaderAssassinated:
                fear_ += ASSASSIN_FEAR_LEVEL;
                if (fear_ > 100.0f) fear_ = 100.0f;
                delta += ASSASSIN_FEAR_POWER;
                break;
            case EventType::ZoneBlightTick:
                fear_ += BLIGHT_TICK_FEAR;
                if (fear_ > 100.0f) fear_ = 100.0f;
                delta += BLIGHT_TICK_POWER;
                break;
            case EventType::ZoneBlighted:
                fear_ += BLIGHT_DONE_FEAR;
                if (fear_ > 100.0f) fear_ = 100.0f;
                delta += BLIGHT_DONE_POWER;
                break;
            case EventType::ChampionSummoned:
                fear_ += CHAMPION_FEAR_LEVEL;
                if (fear_ > 100.0f) fear_ = 100.0f;
                break;
            case EventType::TrapSprung:
                // Traps generate fear too (Trickery synergy).
                fear_ += TRICKERY_TRAP_FEAR;
                if (fear_ > 100.0f) fear_ = 100.0f;
                break;
            default: break;
        }
    }

    if (isActive(Belief::Breeding)) {
        switch (e.type) {
            case EventType::MonstrosityBred:   delta += BREED_SUCCESS; break;
            case EventType::CultistKilledByFeral: delta += BREED_FERAL_KILL; break;
            default: break;
        }
    }

    if (isActive(Belief::Chaos)) {
        switch (e.type) {
            case EventType::Explosion:        delta += CHAOS_EXPLOSION; break;
            case EventType::CultistImprisoned: delta += CHAOS_IMPRISONED; break;
            case EventType::InfringerPunished:
                // Punishing cultists to align them with other beliefs stokes
                // insurrection under Chaos.
                { GameEvent r(EventType::InsurrectionRiskUp);
                  r.amount = PUNISH_RISK_UP; r.tag = "chaos_punish";
                  bus_.publish(r); }
                break;
            default: break;
        }
    }

    if (isActive(Belief::Sacrifice)) {
        switch (e.type) {
            case EventType::SacrificeCompleted:  delta += SACRIFICE_COMPLETED; break;
            case EventType::SacrificeInterrupted: delta += SACRIFICE_INTERRUPTED; break;
            case EventType::DeathDenied:         delta += DEATH_DENIED; break;
            default: break;
        }
    }

    if (isActive(Belief::Conversion)) {
        switch (e.type) {
            case EventType::ConversionPerformed:
                delta += CONVERSION_PER_SOUL * e.amount; break;
            default: break;
        }
    }

    if (isActive(Belief::War)) {
        switch (e.type) {
            case EventType::EnemyCultistSlain:
                delta += (activeWars_ <= 1) ? WAR_SINGLE_KILL
                                            : WAR_SINGLE_KILL * WAR_MULTI_RATE;
                break;
            case EventType::BaseBuildingDamaged:   delta += WAR_BUILDING_DAMAGED; break;
            case EventType::BaseBuildingDestroyed: delta += WAR_BUILDING_DESTROYED; break;
            // Wave 9b: decapitating the enemy cult is an act of war.
            case EventType::LeaderAssassinated: delta += ASSASSIN_WAR_POWER; break;
            default: break;
        }
    }

    if (isActive(Belief::Trickery)) {
        switch (e.type) {
            case EventType::MimicKill:  delta += TRICKERY_MIMIC_KILL; break;
            case EventType::MimicSlain: delta += TRICKERY_MIMIC_SLAIN; break;
            case EventType::TrapSprung: delta += TRICKERY_TRAP_SPRUNG; break;
            default: break;
        }
    }

    if (isActive(Belief::Magic)) {
        switch (e.type) {
            case EventType::NecromancyPerformed: delta += MAGIC_NECROMANCY; break;
            // Wave 9b: the summoning answers (or fizzles).
            case EventType::ChampionSummoned: delta += CHAMPION_MAGIC_POWER; break;
            case EventType::SummoningInterrupted: delta += SUMMON_INTERRUPT_MAGIC; break;
            default: break;
        }
    }

    if (isActive(Belief::Onslaught)) {
        switch (e.type) {
            case EventType::CivilianSlain:
                // Mass-casualty events (city destruction) carry the count in
                // amount; a bare event still counts as one civilian.
                delta += ONSLAUGHT_PER_CIVILIAN *
                         (e.amount > 0.0f ? e.amount : 1.0f);
                onslaughtIdleTime_ = 0.0;
                break;
            default: break;
        }
    }

    if (isActive(Belief::Dreams)) {
        switch (e.type) {
            case EventType::DreamWhisper:
                delta += DREAM_WHISPER_POWER * e.amount; break;
            default: break;
        }
    }

    // Wave 10 (R5): Reconstruction had no power path (only the 1.5x heal
    // multiplier). Rebuilding and mending now feed Cthulhu directly.
    if (isActive(Belief::Reconstruction)) {
        switch (e.type) {
            case EventType::BuildingRebuilt:
                delta += RECONSTRUCTION_PER_REBUILD; break;
            case EventType::HealPerformed:
                delta += RECONSTRUCTION_PER_HEAL; break;
            default: break;
        }
    }

    // Ambient cultist behavior (belief-agnostic prayer power; belief-gated
    // desecration effects).
    switch (e.type) {
        case EventType::PrayerOffered:
            delta += PRAYER_POWER * e.amount; break;
        case EventType::DesecrationDone:
            if (isActive(Belief::Fear)) {
                fear_ += DESECRATE_FEAR;
                if (fear_ > 100.0f) fear_ = 100.0f;
            }
            if (isActive(Belief::Torture)) delta += DESECRATE_TORTURE;
            break;
        // Wave 9c: new ambient activities.
        case EventType::OmenRead:
            delta += OMEN_READ_POWER * e.amount; break;
        case EventType::SigilPainted:
            if (isActive(Belief::Fear)) {
                fear_ += SIGIL_FEAR;
                if (fear_ > 100.0f) fear_ = 100.0f;
            }
            break;
        case EventType::SparringHeld:
            if (isActive(Belief::War) || isActive(Belief::Onslaught))
                delta += SPARRING_POWER * e.amount;
            break;
        case EventType::ChantingHeld:
            if (isActive(Belief::Magic)) delta += CHANT_POWER * e.amount;
            break;
        // Wave 15: new ambient activities.
        case EventType::DreamShared:
            if (isActive(Belief::Dreams))
                delta += DREAMSHARE_POWER * e.amount;
            break;
        case EventType::EffigyMended:
            if (isActive(Belief::Reconstruction))
                delta += EFFIGY_POWER * e.amount;
            break;
        case EventType::RiteOfFlesh:
            if (isActive(Belief::Fear)) {
                fear_ += RITE_FEAR;
                if (fear_ > 100.0f) fear_ = 100.0f;
            }
            break;
        case EventType::WildsHunted:
            if (isActive(Belief::Onslaught))
                delta += HUNT_POWER * e.amount;
            break;
        default: break;
    }

    return delta;
}

float BeliefSystem::tick(double dt) {
    float delta = 0.0f;

    if (isActive(Belief::Fear)) {
        if (inCombat_) {
            // Captured creatures / bred monstrosities in the army generate
            // passive fear while fighting.
            fear_ += fearPassiveGen_ * 0.5f * static_cast<float>(dt);
            if (fear_ > 100.0f) fear_ = 100.0f;
        } else if (fear_ > 0.0f) {
            // Fear decays when not in combat.
            float before = fear_;
            fear_ -= FEAR_DECAY_PER_SEC * static_cast<float>(dt);
            if (fear_ < 0.0f) fear_ = 0.0f;
            delta -= FEAR_POWER_DECAY * static_cast<float>(dt)
                     * (before > 0.0f ? 1.0f : 0.0f);
        }
    }

    if (isActive(Belief::Onslaught)) {
        onslaughtIdleTime_ += dt;
        if (onslaughtIdleTime_ > ONSLAUGHT_IDLE_LIMIT)
            delta -= ONSLAUGHT_IDLE_DECAY * static_cast<float>(dt);
    }

    return delta;
}

bool BeliefSystem::canMassConvert() const {
    return isActive(Belief::Conversion) && clock_.now() >= massConvertReadyAt_;
}

void BeliefSystem::useMassConvert() {
    if (!canMassConvert()) return;
    massConvertReadyAt_ = clock_.now() + MASS_CONVERT_COOLDOWN;
    GameEvent e(EventType::MassConversionUsed);
    e.amount = static_cast<float>(MASS_CONVERT_COUNT);
    bus_.publish(e);
    GameEvent c(EventType::ConversionPerformed);
    c.amount = static_cast<float>(MASS_CONVERT_COUNT);
    c.tag = "mass_convert";
    bus_.publish(c);
}

float BeliefSystem::healMultiplier() const {
    return isActive(Belief::Reconstruction) ? 1.5f : 1.0f;
}
float BeliefSystem::spellEffectiveness() const {
    return isActive(Belief::Reconstruction) ? 0.75f : 1.0f;
}
float BeliefSystem::meleeBonus() const {
    return isActive(Belief::Onslaught) ? 1.25f : 1.0f;
}
float BeliefSystem::sorcererDamageMult() const {
    return isActive(Belief::Magic) ? 1.3f : 1.0f;
}
float BeliefSystem::foeCcDurationMult() const {
    return isActive(Belief::Magic) ? 0.6f : 1.0f;
}
float BeliefSystem::magicLearnTimeMult() const {
    return isActive(Belief::Magic) ? 1.5f : 1.0f;
}
float BeliefSystem::conversionEfficiency() const {
    return isActive(Belief::Conversion) ? 1.5f : 1.0f;
}
float BeliefSystem::conversionVulnerability() const {
    // Conversion makes cultists more prone to being stolen by other deities.
    return isActive(Belief::Conversion) ? 1.5f : 1.0f;
}

} // namespace cultulhu
