#include "commands/DirectiveExecutor.h"

#include "beliefs/Belief.h"
#include "commands/CommandSystem.h"
#include "cult/CultManager.h"
#include "entities/Units.h"
#include "exertion/ExertionSystem.h"
#include "power/PowerSystem.h"
#include "world/WorldMap.h"
#include "world/Zone.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace cultulhu {

// Wave 9b tuning (all creative-liberty numbers).
namespace {
// AssassinateProphet: infiltration approach.
constexpr double ASSASSIN_DURATION = 60.0;   // seconds of approach
constexpr double ASSASSIN_TICK = 5.0;        // one strike attempt per tick
constexpr float ASSASSIN_BASE_CHANCE = 0.08f;
constexpr float ASSASSIN_TRICKERY_WEIGHT = 0.30f; // * Trickery exertion 0..1
constexpr float ASSASSIN_DIST_PENALTY_CAP = 0.15f; // full penalty at 2000m+
constexpr float ASSASSIN_EXPOSE_CHANCE = 0.10f;   // per failed strike tick
constexpr float ASSASSIN_RISK_NUDGE = 4.0f;  // insurrection risk on exposure
constexpr float MORALE_SHOCK_SECONDS = 120.0f;   // enemy morale shock window
// BlightLand: zone corruption.
constexpr double BLIGHT_TICKS = 120.0;       // corruption ticks (full)
constexpr double BLIGHT_TICK_INTERVAL = 1.0; // seconds of game time per tick
constexpr float BLIGHT_TRICKLE_PER_CULTIST = 0.1f; // power/s per cultist in zone
constexpr float BLIGHT_FALLBACK_RADIUS = 60.0f;    // when no zone bounds known
// GrandSummoning: the long ritual.
constexpr double SUMMON_DURATION = 90.0;     // seconds of game time
constexpr double SUMMON_TICK = 5.0;
constexpr float SUMMON_COST = 300.0f;        // power consumed at start
constexpr float SUMMON_INTERRUPT_CHANCE = 0.02f;  // per tick (no refund)
constexpr float CHAMPION_MAX_HP = 1200.0f;   // dread champion (boosted Monstrosity)
} // namespace

// ---------------------------------------------------------------------------
// DirectiveOperation base
// ---------------------------------------------------------------------------

DirectiveOperation::DirectiveOperation(EventBus& bus, RNG& rng,
                                       CultManager& cult,
                                       std::string directiveName,
                                       double durationSeconds,
                                       double tickIntervalSeconds,
                                       bool partial)
    : bus_(bus), rng_(rng), cult_(cult), name_(std::move(directiveName)),
      duration_(durationSeconds), tickInterval_(tickIntervalSeconds),
      partial_(partial) {}

float DirectiveOperation::progress() const {
    if (duration_ <= 0.0) return 1.0f;
    float p = static_cast<float>(elapsed_ / duration_);
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    return p;
}

std::string DirectiveOperation::describe() const {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s %d%%%s", name_.c_str(),
                  static_cast<int>(progress() * 100.0f + 0.5f),
                  partial_ ? " (partial)" : "");
    return buf;
}

void DirectiveOperation::publishProgress() const {
    GameEvent e(EventType::DirectiveProgress);
    e.tag = name_;
    e.amount = progress();
    bus_.publish(e);
}

void DirectiveOperation::finishEarly() { finished_ = true; }

void DirectiveOperation::update(double dt) {
    if (finished_ || dt <= 0.0) return;
    elapsed_ += dt;
    tickAcc_ += dt;
    while (tickAcc_ >= tickInterval_ && !finished_) {
        tickAcc_ -= tickInterval_;
        onTick(); // may call finishEarly() (e.g. sacrifice interruption)
        if (!finished_) publishProgress();
    }
    if (!finished_ && elapsed_ >= duration_) {
        finished_ = true;
        onComplete();
        GameEvent e(EventType::DirectiveCompleted);
        e.tag = name_;
        bus_.publish(e);
    }
}

// ---------------------------------------------------------------------------
// Concrete operations. All gameplay effects go through bus events so the
// exertion/power pipeline reacts automatically.
// ---------------------------------------------------------------------------

class RaidOperation : public DirectiveOperation {
public:
    RaidOperation(EventBus& bus, RNG& rng, CultManager& cult, Vec3 target,
                  bool partial)
        : DirectiveOperation(bus, rng, cult,
                             directiveName(DirectiveType::RaidCity),
                             partial ? 25.0 : 50.0, 5.0, partial),
          target_(target) {}

protected:
    void onTick() override {
        // The RaidPerformed event is what CitySystem.damageBuilding emits
        // when a district takes a hit, so this reads as district damage to
        // every listener (Fear belief, exertion, narration).
        GameEvent e(EventType::RaidPerformed);
        e.amount = rng_.uniform(0.1f, 0.3f) * magnitudeScale();
        e.pos = target_;
        e.tag = "directive_raid";
        bus_.publish(e);
    }
    void onComplete() override {}

private:
    Vec3 target_;
};

class WarOperation : public DirectiveOperation {
public:
    WarOperation(EventBus& bus, RNG& rng, CultManager& cult, int enemyFaction,
                 bool partial)
        : DirectiveOperation(bus, rng, cult,
                             directiveName(DirectiveType::GoToWar),
                             partial ? 20.0 : 40.0, 5.0, partial),
          enemyFaction_(enemyFaction) {}

protected:
    void onTick() override {
        GameEvent e(EventType::EnemyCultistSlain);
        e.amount = static_cast<float>(partial() ? rng_.intRange(1, 2)
                                                : rng_.intRange(1, 3));
        e.faction = enemyFaction_;
        e.tag = "directive_war";
        bus_.publish(e);
    }
    void onComplete() override {}

private:
    int enemyFaction_;
};

class ConvertOperation : public DirectiveOperation {
public:
    ConvertOperation(EventBus& bus, RNG& rng, CultManager& cult, bool partial)
        : DirectiveOperation(bus, rng, cult,
                             directiveName(DirectiveType::ConvertCampaign),
                             partial ? 15.0 : 30.0, 5.0, partial) {}

protected:
    void onTick() override {
        GameEvent e(EventType::ConversionPerformed);
        e.amount = static_cast<float>(partial() ? 1 : rng_.intRange(1, 2));
        e.tag = "directive_convert";
        bus_.publish(e);
    }
    void onComplete() override {}
};

class SacrificeOperation : public DirectiveOperation {
public:
    SacrificeOperation(EventBus& bus, RNG& rng, CultManager& cult,
                       bool partial)
        : DirectiveOperation(bus, rng, cult,
                             directiveName(DirectiveType::MassSacrifice),
                             partial ? 15.0 : 30.0, 5.0, partial) {}

protected:
    void onTick() override {
        bool anyAlive = false;
        for (size_t i = 0; i < cult_.size(); ++i) {
            if (cult_.at(i).alive()) { anyAlive = true; break; }
        }
        // No living cultists to sacrifice, or the ritual is disrupted:
        // the sacrifice fizzles instead of completing.
        if (!anyAlive || rng_.chance(0.05f)) {
            GameEvent e(EventType::SacrificeInterrupted);
            e.tag = name();
            bus_.publish(e);
            finishEarly();
        }
    }
    void onComplete() override {
        GameEvent e(EventType::SacrificeCompleted);
        e.tag = name();
        bus_.publish(e);
    }
};

class DefendOperation : public DirectiveOperation {
public:
    DefendOperation(EventBus& bus, RNG& rng, CultManager& cult, bool partial)
        : DirectiveOperation(bus, rng, cult,
                             directiveName(DirectiveType::Defend),
                             partial ? 30.0 : 60.0, 10.0, partial) {}

protected:
    // Deliberately no gameplay event per tick: the defense buff is applied
    // by the game layer via DirectiveExecutor::defenseActive(). Progress
    // and completion events are published by the base class.
    void onTick() override {}
    void onComplete() override {}
};

class RelicOperation : public DirectiveOperation {
public:
    RelicOperation(EventBus& bus, RNG& rng, CultManager& cult, bool partial)
        : DirectiveOperation(bus, rng, cult,
                             directiveName(DirectiveType::GatherRelic),
                             partial ? 15.0 : 30.0, 5.0, partial) {}

protected:
    void onTick() override {} // travel; progress events come from the base
    void onComplete() override {
        // The expedition either returns with a relic or empty-handed.
        // ArtifactTriggered is the existing relic/artifact event
        // (RelicSystem emits it; Trickery exertion feeds on it).
        if (rng_.chance(0.6f)) {
            GameEvent e(EventType::ArtifactTriggered);
            e.tag = "relic";
            e.amount = 1.0f;
            bus_.publish(e);
        }
    }
};

// ---------------------------------------------------------------------------
// Wave 9b: new directive operations.
// ---------------------------------------------------------------------------

class AssassinateOperation : public DirectiveOperation {
public:
    AssassinateOperation(EventBus& bus, RNG& rng, CultManager& cult,
                         Vec3 target, int enemyFaction,
                         const DirectiveContext& ctx, bool partial)
        : DirectiveOperation(bus, rng, cult,
                             directiveName(DirectiveType::AssassinateProphet),
                             partial ? ASSASSIN_DURATION * 0.5
                                     : ASSASSIN_DURATION,
                             ASSASSIN_TICK, partial),
          target_(target), enemyFaction_(enemyFaction), ctx_(ctx) {
        // The infiltrator: the most devoted loyal cultist still standing.
        float best = -1.0f;
        for (size_t i = 0; i < cult_.size(); ++i) {
            const Cultist& c = cult_.at(i);
            if (!c.alive() || c.state() != CultistState::Loyal) continue;
            if (c.devotion() > best) {
                best = c.devotion();
                assassinId_ = c.id();
            }
        }
    }

protected:
    void onTick() override {
        // Per-tick strike chance: Trickery exertion opens the way, distance
        // closes it. Partial obedience halves the crew's competence.
        float trickery = 0.0f;
        if (ctx_.exertion)
            trickery = ctx_.exertion->exertion(Belief::Trickery) / 100.0f;
        Vec3 centroid;
        size_t n = 0;
        for (size_t i = 0; i < cult_.size(); ++i) {
            const Cultist& c = cult_.at(i);
            if (!c.alive()) continue;
            centroid = centroid + c.position();
            ++n;
        }
        float distPenalty = 0.0f;
        if (n > 0) {
            const float d =
                centroid.distance(target_); // Vec3::distance is 3D
            distPenalty = std::min(ASSASSIN_DIST_PENALTY_CAP, d / 2000.0f);
        }
        float p = (ASSASSIN_BASE_CHANCE + ASSASSIN_TRICKERY_WEIGHT * trickery -
                   distPenalty) *
                  magnitudeScale();
        if (p < 0.02f) p = 0.02f;
        if (p > 0.60f) p = 0.60f;

        if (rng_.chance(p)) {
            // The blade finds its mark: the leader dies, their cult's
            // morale breaks (conversions against them ease while the
            // Conversion-exertion this feeds stays high).
            GameEvent kill(EventType::LeaderAssassinated);
            kill.sourceId = ctx_.targetEntityId;
            kill.faction = enemyFaction_;
            kill.amount = 1.0f;
            kill.pos = target_;
            bus_.publish(kill);
            GameEvent shock(EventType::EnemyMoraleShocked);
            shock.faction = enemyFaction_;
            shock.amount = MORALE_SHOCK_SECONDS;
            bus_.publish(shock);
            finishEarly();
            return;
        }
        if (rng_.chance(ASSASSIN_EXPOSE_CHANCE)) {
            // Exposed: the target escapes, the cult fears retaliation, and
            // the enemy deity notices (CombatStarted reads as a War event).
            GameEvent exposed(EventType::AssassinExposed);
            exposed.sourceId = assassinId_;
            exposed.tag = name();
            bus_.publish(exposed);
            GameEvent risk(EventType::InsurrectionRiskUp);
            risk.amount = ASSASSIN_RISK_NUDGE;
            risk.tag = "assassin_exposed";
            bus_.publish(risk);
            GameEvent war(EventType::CombatStarted);
            war.faction = enemyFaction_;
            war.tag = "deity_noticed";
            war.pos = target_;
            bus_.publish(war);
            finishEarly();
        }
    }

    void onComplete() override {
        // The infiltration never found an opening and dissolves quietly.
    }

private:
    Vec3 target_;
    int enemyFaction_;
    DirectiveContext ctx_;
    uint64_t assassinId_ = 0;
};

class BlightOperation : public DirectiveOperation {
public:
    BlightOperation(EventBus& bus, RNG& rng, CultManager& cult, Vec3 target,
                    const DirectiveContext& ctx, bool partial)
        : DirectiveOperation(bus, rng, cult,
                             directiveName(DirectiveType::BlightLand),
                             (partial ? BLIGHT_TICKS * 0.5 : BLIGHT_TICKS) *
                                 BLIGHT_TICK_INTERVAL,
                             BLIGHT_TICK_INTERVAL, partial),
          ctx_(ctx), center_(target) {
        // Resolve the zone at the target: name + bounds for the corruption.
        if (ctx_.worldMap) {
            if (const Zone* z = ctx_.worldMap->zoneAt(target)) {
                zoneName_ = z->name();
                zmin_ = z->min();
                zmax_ = z->max();
                hasBounds_ = true;
                center_ = z->center();
            }
        }
    }

protected:
    void onTick() override {
        // The city system reads ZoneBlightTick to scale civilian output
        // down while the blight spreads; Fear exertion feeds off every tick.
        GameEvent t(EventType::ZoneBlightTick);
        t.tag = zoneName_;
        t.amount = progress();
        t.pos = center_;
        bus_.publish(t);
        // Cultists standing in the blight draw a small power trickle.
        if (ctx_.power) {
            int inZone = 0;
            for (size_t i = 0; i < cult_.size(); ++i) {
                const Cultist& c = cult_.at(i);
                if (!c.alive()) continue;
                if (inBlight(c.position())) ++inZone;
            }
            if (inZone > 0)
                ctx_.power->add(BLIGHT_TRICKLE_PER_CULTIST *
                                static_cast<float>(inZone) *
                                magnitudeScale());
        }
    }

    void onComplete() override {
        // Persistent state: the zone is Blighted from now on.
        bool flagged = false;
        if (ctx_.worldMap) flagged = ctx_.worldMap->blightZone(zoneName_);
        GameEvent e(EventType::ZoneBlighted);
        e.tag = zoneName_;
        e.amount = flagged ? 1.0f : 0.0f;
        e.pos = center_;
        bus_.publish(e);
    }

private:
    bool inBlight(Vec3 p) const {
        if (hasBounds_) {
            return p.x >= zmin_.x && p.x <= zmax_.x &&
                   p.y >= zmin_.y && p.y <= zmax_.y &&
                   p.z >= zmin_.z && p.z <= zmax_.z;
        }
        const float dx = p.x - center_.x, dz = p.z - center_.z;
        return dx * dx + dz * dz <=
               BLIGHT_FALLBACK_RADIUS * BLIGHT_FALLBACK_RADIUS;
    }

    DirectiveContext ctx_;
    std::string zoneName_ = "the wilds";
    Vec3 center_;
    Vec3 zmin_;
    Vec3 zmax_;
    bool hasBounds_ = false;
};

class SummoningOperation : public DirectiveOperation {
public:
    SummoningOperation(EventBus& bus, RNG& rng, CultManager& cult, Vec3 site,
                       const DirectiveContext& ctx, bool partial)
        : DirectiveOperation(bus, rng, cult,
                             directiveName(DirectiveType::GrandSummoning),
                             partial ? SUMMON_DURATION * 0.5 : SUMMON_DURATION,
                             SUMMON_TICK, partial),
          site_(site) {
        // The price is paid up front: 300 power, or the rite cannot begin.
        if (!ctx.power || ctx.power->value() < SUMMON_COST) {
            GameEvent e(EventType::SummoningInterrupted);
            e.tag = "insufficient_power";
            e.pos = site_;
            bus_.publish(e);
            finishEarly();
            return;
        }
        ctx.power->add(-SUMMON_COST);
    }

protected:
    void onTick() override {
        // Long rituals draw attention: each tick risks disruption (like
        // SacrificeOperation). Spent power is NOT refunded.
        if (rng_.chance(SUMMON_INTERRUPT_CHANCE)) {
            GameEvent e(EventType::SummoningInterrupted);
            e.tag = name();
            e.pos = site_;
            bus_.publish(e);
            finishEarly();
        }
    }

    void onComplete() override {
        // Something vast answers. The game layer spawns the entity from
        // this event: a boosted Monstrosity (the dread champion).
        GameEvent e(EventType::ChampionSummoned);
        e.sourceId = 0; // the game layer spawns the entity
        e.tag = "dread_champion";
        e.amount = partial() ? CHAMPION_MAX_HP * 0.5f : CHAMPION_MAX_HP;
        e.pos = site_;
        bus_.publish(e);
    }

private:
    Vec3 site_;
};

// ---------------------------------------------------------------------------
// DirectiveExecutor
// ---------------------------------------------------------------------------

static DirectiveType directiveTypeByName(const std::string& name) {
    for (int i = 0; i < static_cast<int>(DirectiveType::Count); ++i) {
        DirectiveType d = static_cast<DirectiveType>(i);
        if (name == directiveName(d)) return d;
    }
    return DirectiveType::Count;
}

DirectiveExecutor::DirectiveExecutor(EventBus& bus, RNG& rng, CultManager& cult)
    : bus_(bus), rng_(rng), cult_(cult) {
    bus_.subscribe(EventType::DirectiveResolved,
                   [this](const GameEvent& e) { onResolved(e); });
}

void DirectiveExecutor::spawn(std::unique_ptr<DirectiveOperation> op) {
    if (ops_.size() >= MAX_OPERATIONS) {
        std::fprintf(stderr,
                     "[directive] at operation cap (%zu); '%s' not started\n",
                     MAX_OPERATIONS, op->name().c_str());
        return;
    }
    ops_.push_back(std::move(op));
}

void DirectiveExecutor::onResolved(const GameEvent& e) {
    // Tag format from CommandSystem: "DirectiveName/OutcomeName".
    const size_t slash = e.tag.find('/');
    if (slash == std::string::npos) return;
    const std::string dname = e.tag.substr(0, slash);
    const std::string oname = e.tag.substr(slash + 1);

    // Only carried-out directives get follow-through.
    if (oname != commandOutcomeName(CommandOutcome::Obeyed) &&
        oname != commandOutcomeName(CommandOutcome::PartiallyObeyed))
        return; // Refused / SparksInsurrection: no operation

    const bool partial = (oname == commandOutcomeName(CommandOutcome::PartiallyObeyed));
    const DirectiveType d = directiveTypeByName(dname);
    if (d == DirectiveType::Count) return; // unknown directive name

    const int enemyFaction = e.faction >= 0 ? e.faction : 1;
    switch (d) {
        case DirectiveType::GoToWar:
            spawn(std::make_unique<WarOperation>(bus_, rng_, cult_,
                                                 enemyFaction, partial));
            break;
        case DirectiveType::RaidCity:
            spawn(std::make_unique<RaidOperation>(bus_, rng_, cult_, e.pos,
                                                  partial));
            break;
        case DirectiveType::ConvertCampaign:
            spawn(std::make_unique<ConvertOperation>(bus_, rng_, cult_,
                                                     partial));
            break;
        case DirectiveType::MassSacrifice:
            spawn(std::make_unique<SacrificeOperation>(bus_, rng_, cult_,
                                                       partial));
            break;
        case DirectiveType::Defend:
            spawn(std::make_unique<DefendOperation>(bus_, rng_, cult_,
                                                    partial));
            break;
        case DirectiveType::GatherRelic:
            spawn(std::make_unique<RelicOperation>(bus_, rng_, cult_,
                                                   partial));
            break;
        // Wave 9b.
        case DirectiveType::AssassinateProphet:
            spawn(std::make_unique<AssassinateOperation>(bus_, rng_, cult_,
                                                         e.pos, enemyFaction,
                                                         ctx_, partial));
            break;
        case DirectiveType::BlightLand:
            spawn(std::make_unique<BlightOperation>(bus_, rng_, cult_, e.pos,
                                                    ctx_, partial));
            break;
        case DirectiveType::GrandSummoning:
            spawn(std::make_unique<SummoningOperation>(bus_, rng_, cult_,
                                                       e.pos, ctx_, partial));
            break;
        case DirectiveType::Count:
            break;
    }
}

void DirectiveExecutor::update(double dt) {
    for (auto& op : ops_) op->update(dt);
    ops_.erase(std::remove_if(ops_.begin(), ops_.end(),
                              [](const std::unique_ptr<DirectiveOperation>& op) {
                                  return op->done();
                              }),
               ops_.end());
}

bool DirectiveExecutor::defenseActive() const {
    const std::string defend = directiveName(DirectiveType::Defend);
    for (const auto& op : ops_) {
        if (!op->done() && op->name() == defend) return true;
    }
    return false;
}

} // namespace cultulhu
