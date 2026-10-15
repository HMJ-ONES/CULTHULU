// CULT-ULHU micro-dev regression tests: AbilityCast events stamp the
// caster's position.
//
// Five publish sites in src/modes/MatchKits.cpp (Q/F/R spell casts, RMB
// heavy attacks, rock rip / break / swing) used to publish AbilityCast
// with no pos: the only AbilityCast site stamping pos was the MOBA
// summoner in MobaDefense.cpp. Any engine-side cast visual resolved from
// those events landed at the world origin — the same "worse than
// silence" class the FX sweep already closed for SacrificeStarted and
// BrawlBrokeOut. These pin, through the production match loop:
//
// 1. A Q spell cast (Tentacle Slam) publishes AbilityCast, slot 0, with
//    pos == the caster's position.
// 2. An RMB heavy attack (Wave of Domination, MindControl gaze)
//    publishes AbilityCast, slot 3, with pos == the caster's position.
// 3. Ripping Cthulhu's rock publishes "Sunder the Earth" with pos.
// 4. A landed rock swing publishes "rock swing" with pos.
// 5. Hurling the rock (the break path) publishes "Hurl the Mountain"
//    with pos.

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                     \
            ++failures;                                    \
        }                                                  \
    } while (0)

static bool near(const Vec3& a, const Vec3& b) {
    return std::fabs(a.x - b.x) < 1e-4f &&
           std::fabs(a.y - b.y) < 1e-4f &&
           std::fabs(a.z - b.z) < 1e-4f;
}

static const GameEvent* findCast(const std::vector<GameEvent>& v,
                                 const std::string& tag) {
    for (const auto& e : v)
        if (e.tag == tag) return &e;
    return nullptr;
}

int main() {
    // 1/3/4/5: Q cast, rock rip, rock swing, rock break — all in one
    // Domination match.
    {
        EventBus bus; GameClock clock; RNG rng(11);
        std::vector<GameEvent> casts;
        bus.subscribe(EventType::AbilityCast,
                      [&casts](const GameEvent& e) { casts.push_back(e); });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        const uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "cthulhu_avatar"));
        const uint64_t h = m.joinAsHuman(0); // human takes over a
        CHECK(h == a);
        const size_t ih = (m.player(0).id == h) ? 0 : 1;
        const Vec3 at(3.0f, 0.0f, 4.0f);
        m.movePlayer(h, at);
        m.movePlayer(b, Vec3(7.0f, 0.0f, 4.0f)); // 4m ahead, in the cone
        CHECK(m.setFacingYaw(h, 0.0f));

        // 1: Q cast through the public path.
        CHECK(m.humanCast(h, 0, 0.0f));
        const GameEvent* q = findCast(casts, "Tentacle Slam");
        CHECK(q != nullptr);
        if (q) {
            CHECK(q->amount == 0.0f);
            CHECK(near(q->pos, m.player(ih).pos));
        }

        // 3: rip the rock through the public RMB-down path.
        casts.clear();
        CHECK(m.humanCast(h, 3, 0.0f, Match::RmbAim::Down));
        const GameEvent* rip = findCast(casts, "Sunder the Earth");
        CHECK(rip != nullptr);
        if (rip) {
            CHECK(rip->amount == 3.0f);
            CHECK(near(rip->pos, m.player(ih).pos));
        }
        CHECK(m.player(ih).hasRock);

        // 4: a landed swing through the public RMB path (rock in hand +
        // enemy in range, durability 3 -> 2, so the swing tag — not the
        // break tag — fires).
        casts.clear();
        CHECK(m.humanCast(h, 3, 0.0f, Match::RmbAim::Normal));
        const GameEvent* swing = findCast(casts, "rock swing");
        CHECK(swing != nullptr);
        if (swing) {
            CHECK(swing->amount == 3.0f);
            CHECK(near(swing->pos, m.player(ih).pos));
        }
    }

    // 5: the break path (hurl) through the public RMB-launch path.
    {
        EventBus bus; GameClock clock; RNG rng(29);
        std::vector<GameEvent> casts;
        bus.subscribe(EventType::AbilityCast,
                      [&casts](const GameEvent& e) { casts.push_back(e); });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        const uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "cthulhu_avatar"));
        const uint64_t h = m.joinAsHuman(0);
        CHECK(h == a);
        const size_t ih = (m.player(0).id == h) ? 0 : 1;
        m.movePlayer(h, Vec3(3.0f, 0.0f, 4.0f));
        m.movePlayer(b, Vec3(7.0f, 0.0f, 4.0f));
        CHECK(m.setFacingYaw(h, 0.0f));
        CHECK(m.humanCast(h, 3, 0.0f, Match::RmbAim::Down));
        CHECK(m.player(ih).hasRock);
        casts.clear();
        // Launch: the hurl lands on the enemy in the cone, spending the
        // rock through breakRock with the "Hurl the Mountain" tag.
        CHECK(m.humanCast(h, 3, 0.0f, Match::RmbAim::Launch));
        CHECK(!m.player(ih).hasRock);
        const GameEvent* brk = findCast(casts, "Hurl the Mountain");
        CHECK(brk != nullptr);
        if (brk) CHECK(near(brk->pos, m.player(ih).pos));
    }

    // 2: the standard RMB heavy attack (no rock) stamps pos as well.
    {
        EventBus bus; GameClock clock; RNG rng(23);
        std::vector<GameEvent> casts;
        bus.subscribe(EventType::AbilityCast,
                      [&casts](const GameEvent& e) { casts.push_back(e); });
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        const uint64_t a = m.addBot(0);
        const uint64_t b = m.addBot(1);
        CHECK(m.setPlayerKit(a, "cthulhu_avatar"));
        const uint64_t h = m.joinAsHuman(0);
        CHECK(h == a);
        const size_t ih = (m.player(0).id == h) ? 0 : 1;
        const Vec3 at(-5.0f, 0.0f, 9.0f);
        m.movePlayer(h, at);
        m.movePlayer(b, Vec3(0.0f, 0.0f, 9.0f)); // 5m ahead, in the cone
        CHECK(m.setFacingYaw(h, 0.0f));
        CHECK(m.humanCast(h, 3, 0.0f, Match::RmbAim::Normal));
        const GameEvent* rmb = findCast(casts, "Wave of Domination");
        CHECK(rmb != nullptr);
        if (rmb) {
            CHECK(rmb->amount == 3.0f);
            CHECK(near(rmb->pos, m.player(ih).pos));
        }
    }

    std::cout << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
