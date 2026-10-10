#include "sim/ScenarioBot.h"

#include "sim/SimWorld.h"

#include <algorithm>

namespace cultulhu {
namespace sim {

ScenarioBot::ScenarioBot(SimWorld& world, const std::vector<Belief>& loadout)
    : w_(world), loadout_(loadout) {}

bool ScenarioBot::has(Belief b) const {
    return std::find(loadout_.begin(), loadout_.end(), b) != loadout_.end();
}

void ScenarioBot::deathEvent(EventType type, const std::string& causeTag) {
    if (!w_.killOneCultist()) return; // no victims left: no event
    GameEvent e(type);
    e.amount = 1.0f;
    e.tag = causeTag;
    w_.bus().publish(e);
}

void ScenarioBot::ambient(int t) {
    // Baseline cult life: prayers and the occasional desecration. Prayer
    // power is belief-agnostic, so every scenario gets the same floor.
    if (t % 40 == 20) {
        GameEvent e(EventType::PrayerOffered);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 500 == 250) {
        GameEvent e(EventType::DesecrationDone);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
}

void ScenarioBot::torture(int t) {
    if (t % 40 == 0) {
        GameEvent e(EventType::TorturePerformed);
        // Usually one victim; sometimes a pair is dragged in.
        e.amount = w_.rng().chance(0.30f) ? 2.0f : 1.0f;
        w_.bus().publish(e);
    }
    if (t % 200 == 100) {
        GameEvent e(EventType::CapturedTortured);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 400 == 200) deathEvent(EventType::CultistOneHitKilled, "one_hit");
}

void ScenarioBot::fear(int t) {
    if (t % 100 == 0) {
        GameEvent e(EventType::RaidPerformed);
        e.amount = 0.55f; // destruction level of the raid
        w_.bus().publish(e);
    }
    if (t % 700 == 350) deathEvent(EventType::CultistLost, "lost");
}

void ScenarioBot::breeding(int t) {
    if (t % 150 == 0) {
        GameEvent e(EventType::MonstrosityBred);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 800 == 400) {
        GameEvent r(EventType::FeralRampage);
        w_.bus().publish(r);
        deathEvent(EventType::CultistKilledByFeral, "feral");
    }
}

void ScenarioBot::chaos(int t) {
    if (t % 80 == 0) {
        GameEvent e(EventType::Explosion);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 600 == 300) {
        GameEvent e(EventType::LunaticActed);
        e.tag = "interruptedRitual";
        w_.bus().publish(e);
    }
    if (t % 900 == 450) {
        GameEvent e(EventType::CultistImprisoned);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    // The cult keeps discipline: punishing infringers stokes insurrection
    // risk (doubly so under Chaos), exercising the revolt path.
    if (t % 500 == 100) w_.beliefs().punishInfringer();
}

void ScenarioBot::sacrifice(int t) {
    if (t % 300 == 0) {
        GameEvent e(EventType::SacrificeCompleted);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 1200 == 600) {
        GameEvent e(EventType::SacrificeInterrupted);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    // A cultist offered on the altar.
    if (t % 1500 == 750) deathEvent(EventType::SacrificeCompleted, "sacrificed");
}

void ScenarioBot::conversion(int t) {
    if (t % 120 == 0) {
        GameEvent e(EventType::ConversionPerformed);
        e.amount = 2.0f;
        w_.bus().publish(e);
    }
    if (t % 250 == 125) {
        GameEvent e(EventType::SermonPreached);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    // Mass conversion the moment it is off cooldown.
    if (w_.beliefs().canMassConvert()) w_.beliefs().useMassConvert();
}

void ScenarioBot::war(int t) {
    if (t % 90 == 0) {
        GameEvent e(EventType::EnemyCultistSlain);
        e.faction = 1;
        w_.bus().publish(e);
    }
    if (t % 1200 == 600) {
        GameEvent e(EventType::BaseBuildingDamaged);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 2400 == 1200) {
        GameEvent e(EventType::BaseBuildingDestroyed);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
}

void ScenarioBot::reconstruction(int t) {
    if (t % 200 == 0) {
        GameEvent e(EventType::BuildingRebuilt);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 60 == 30) {
        GameEvent e(EventType::HealPerformed);
        e.amount = 50.0f;
        w_.bus().publish(e);
    }
}

void ScenarioBot::trickery(int t) {
    if (t % 70 == 0) {
        GameEvent e(EventType::TrapSprung);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 250 == 0) {
        GameEvent e(EventType::MimicKill);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 1000 == 500) {
        GameEvent e(EventType::ArtifactTriggered);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
}

void ScenarioBot::magic(int t) {
    if (t % 110 == 0) {
        GameEvent e(EventType::NecromancyPerformed);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 50 == 25) {
        GameEvent e(EventType::SpellCast);
        e.tag = "shadow";
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
}

void ScenarioBot::onslaught(int t) {
    if (t % 50 == 0) {
        GameEvent e(EventType::CivilianSlain);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 150 == 75) {
        GameEvent e(EventType::MeleeAttack);
        e.amount = 10.0f;
        w_.bus().publish(e);
    }
}

void ScenarioBot::dreams(int t) {
    if (t % 200 == 0) {
        GameEvent s(EventType::RestStarted);
        s.sourceId = 1;
        w_.bus().publish(s);
        GameEvent e(EventType::RestEnded);
        e.sourceId = 1;
        w_.bus().publish(e);
    }
    if (t % 150 == 50) {
        GameEvent e(EventType::DreamWhisper);
        e.amount = 1.0f;
        w_.bus().publish(e);
    }
    if (t % 700 == 350) {
        GameEvent e(EventType::Nightmare);
        e.sourceId = 1;
        w_.bus().publish(e);
    }
}

void ScenarioBot::tick(int t) {
    ambient(t);
    if (has(Belief::Torture)) torture(t);
    if (has(Belief::Fear)) fear(t);
    if (has(Belief::Breeding)) breeding(t);
    if (has(Belief::Chaos)) chaos(t);
    if (has(Belief::Sacrifice)) sacrifice(t);
    if (has(Belief::Conversion)) conversion(t);
    if (has(Belief::War)) war(t);
    if (has(Belief::Reconstruction)) reconstruction(t);
    if (has(Belief::Trickery)) trickery(t);
    if (has(Belief::Magic)) magic(t);
    if (has(Belief::Onslaught)) onslaught(t);
    if (has(Belief::Dreams)) dreams(t);
}

} // namespace sim
} // namespace cultulhu
