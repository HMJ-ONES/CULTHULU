// CULT-ULHU wave 7c tests: character framework + KDA.
// Covers: CharacterRegistry register/get/list/duplicate rejection,
// CthulhuAvatar definition sanity, PlayerStatsTracker kill/death/assist
// rules (10s assist window), and the PlayerKda protocol round-trip.

#include "characters/CharacterRegistry.h"
#include "characters/CthulhuAvatar.h"
#include "net/PlayerStats.h"
#include "net/Protocol.h"

#include <cmath>
#include <iostream>

using namespace cultulhu;
using namespace cultulhu::net;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_CLOSE(a, b, eps) do { ++checks; if (std::fabs((a) - (b)) > (eps)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #a " (" << (a) \
    << ") != " #b " (" << (b) << ")\n"; } } while (0)

// ---------------- CharacterRegistry ----------------

static void testRegistry() {
    CharacterRegistry reg;
    CHECK(reg.count() == 0);

    CharacterDef c = makeCthulhuAvatar();
    CHECK(reg.registerCharacter(c));
    CHECK(reg.count() == 1);

    // Duplicate id rejected.
    CHECK(!reg.registerCharacter(makeCthulhuAvatar()));
    CHECK(reg.count() == 1);

    // Empty id rejected.
    CharacterDef empty;
    CHECK(!reg.registerCharacter(empty));

    const CharacterDef* got = reg.get("cthulhu_avatar");
    CHECK(got != nullptr);
    CHECK(got->displayName == "Cthulhu, the Dreaming God");
    CHECK(reg.get("no_such_id") == nullptr);

    auto ids = reg.list();
    CHECK(ids.size() == 1);
    CHECK(ids[0] == "cthulhu_avatar");
}

// ---------------- CthulhuAvatar ----------------

static void testCthulhuAvatar() {
    CharacterDef d = makeCthulhuAvatar();
    CHECK(d.id == "cthulhu_avatar");
    CHECK_CLOSE(d.maxHp, 500.0f, 1e-6);
    CHECK_CLOSE(d.moveSpeed, 6.0f, 1e-6);
    CHECK_CLOSE(d.maxStamina, 100.0f, 1e-6);

    CHECK(d.qAbility.name == "Tentacle Slam");
    CHECK(d.qAbility.effectKind == "aoe_damage");
    CHECK(d.qAbility.effectPower > 0.0f);
    CHECK(d.fAbility.effectKind == "fear_aura");
    CHECK(d.rAbility.effectKind == "summon");

    CHECK(d.meleeComboId == "eldritch_flurry");

    CHECK(d.rightClick.kind == HeavyAttackKind::MindControl);
    CHECK(d.rightClick.name == "Wave of Domination");  // steer: replaces generic RMB
    CHECK(d.rightClick.ccType == "Stun");             // direct-target alternate cast
    CHECK(d.rightClick.ccSeconds > 0.0f);
    CHECK(std::string(heavyAttackKindName(HeavyAttackKind::MindControl)) ==
          "MindControl");

    CHECK(!d.passiveId.empty());
    CHECK(!d.passiveDesc.empty());
}

// ---------------- PlayerStatsTracker ----------------

static void testKdaBasicKill() {
    PlayerStatsTracker t;
    t.setPlayerName(0, "Alice");
    t.setPlayerName(1, "Bob");

    // Bob softens the victim up, Alice lands the kill.
    t.recordDamage(1, /*victimEntity=*/42, 30.0f, /*now=*/100.0);
    t.recordKill(0, 1, 42, 100.5);

    auto rows = t.rows();
    CHECK(rows.size() == 2);
    CHECK(rows[0].name == "Alice");
    CHECK(rows[0].kills == 1);
    CHECK(rows[0].deaths == 0);
    CHECK(rows[0].assists == 0);  // killer never self-assists
    CHECK(rows[1].deaths == 1);
    CHECK(rows[1].assists == 1);   // Bob's damage was in-window
}

static void testKdaStaleDamageNoAssist() {
    PlayerStatsTracker t;
    t.recordDamage(1, 7, 10.0f, 0.0);
    // 11 s later: outside the 10 s assist window.
    t.recordKill(0, 1, 7, 11.0);
    auto rows = t.rows();
    // rows sorted by index: 0 killer, 1 victim.
    CHECK(rows.size() == 2);
    CHECK(rows[0].kills == 1);
    CHECK(rows[1].deaths == 1);
    CHECK(rows[1].assists == 0);
}

static void testKdaTwoDamagersOneKill() {
    PlayerStatsTracker t;
    t.recordDamage(1, 9, 20.0f, 50.0);
    t.recordDamage(2, 9, 20.0f, 55.0);
    t.recordDamage(2, 9, 20.0f, 58.0);  // two hits: still one assist
    t.recordKill(0, 3, 9, 59.0);
    auto rows = t.rows();
    CHECK(rows.size() == 4);  // indices 0,1,2,3
    CHECK(rows[1].assists == 1);
    CHECK(rows[2].assists == 1);
    CHECK(rows[0].kills == 1);
    CHECK(rows[3].deaths == 1);
}

static void testKdaTwoArgOverloadNoAssist() {
    PlayerStatsTracker t;
    t.recordDamage(1, 5, 10.0f, 20.0);
    t.recordKill(0, 1);  // no entity id: no assist lookup
    auto rows = t.rows();
    CHECK(rows[0].kills == 1);
    CHECK(rows[1].deaths == 1);
    CHECK(rows[1].assists == 0);
}

static void testKdaDeadEntityForgotten() {
    PlayerStatsTracker t;
    t.recordDamage(1, 11, 10.0f, 30.0);
    t.recordKill(0, 2, 11, 31.0);  // assist awarded
    // Same entity id "respawns" and dies to someone else without new
    // damage: no stale assist.
    t.recordKill(3, 4, 11, 32.0);
    auto rows = t.rows();
    int assists1 = 0;
    for (const auto& r : rows)
        if (r.name == "Player1") assists1 = r.assists;
    CHECK(assists1 == 1);
    // Player 3's kill must not have awarded a phantom assist to anyone.
    for (const auto& r : rows) CHECK(r.assists <= 1);
}

// ---------------- Protocol: PlayerKda round-trip ----------------

static void testPlayerKdaRoundTrip() {
    std::vector<KdaEntry> in = {
        {0, {"Alice", 3, 1, 2}},
        {1, {"Bob", 0, 4, 0}},
        {9, {"Nyarlathotep", 12, 0, 7}},
    };
    Message m = encodePlayerKda(in);
    CHECK(m.type == MsgType::PlayerKda);

    std::vector<KdaEntry> out;
    CHECK(decodePlayerKda(m, out));
    CHECK(out.size() == 3);
    CHECK(out[0].playerIdx == 0);
    CHECK(out[0].row.name == "Alice");
    CHECK(out[0].row.kills == 3);
    CHECK(out[0].row.deaths == 1);
    CHECK(out[0].row.assists == 2);
    CHECK(out[2].playerIdx == 9);
    CHECK(out[2].row.kills == 12);

    // Full wire frame round-trip (framing + field sanitize).
    auto bytes = encode(m);
    auto back = decodeFrame(bytes.data(), bytes.size());
    CHECK(back.has_value());
    std::vector<KdaEntry> out2;
    CHECK(decodePlayerKda(*back, out2));
    CHECK(out2.size() == 3);
    CHECK(out2[1].row.name == "Bob");
    CHECK(out2[1].row.deaths == 4);

    // Wrong message type rejected.
    Message other{MsgType::ChatMsg, {}};
    std::vector<KdaEntry> junk;
    CHECK(!decodePlayerKda(other, junk));
}

static void testPlayerKdaNameSanitize() {
    std::vector<KdaEntry> in = {{0, {"Al;ice,=Bob", 1, 0, 0}}};
    Message m = encodePlayerKda(in);
    auto bytes = encode(m);
    auto back = decodeFrame(bytes.data(), bytes.size());
    CHECK(back.has_value());
    std::vector<KdaEntry> out;
    CHECK(decodePlayerKda(*back, out));
    CHECK(out.size() == 1);
    CHECK(out[0].row.name == "AliceBob");  // ',' ';' '=' stripped
}

int main() {
    testRegistry();
    testCthulhuAvatar();
    testKdaBasicKill();
    testKdaStaleDamageNoAssist();
    testKdaTwoDamagersOneKill();
    testKdaTwoArgOverloadNoAssist();
    testKdaDeadEntityForgotten();
    testPlayerKdaRoundTrip();
    testPlayerKdaNameSanitize();

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
