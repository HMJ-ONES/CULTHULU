#include "commands/DirectiveExecutor.h"

#include "commands/CommandSystem.h"
#include "cult/CultManager.h"

#include <algorithm>
#include <cstdio>

namespace cultulhu {

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
