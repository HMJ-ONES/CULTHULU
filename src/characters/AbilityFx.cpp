#include "characters/AbilityFx.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace cultulhu {

namespace {

std::string trim(std::string s) {
    const char* ws = " \t\r\n";
    s.erase(0, s.find_first_not_of(ws));
    if (!s.empty()) s.erase(s.find_last_not_of(ws) + 1);
    return s;
}

bool parseBool(const std::string& s, bool& out) {
    if (s == "true" || s == "yes" || s == "1") { out = true; return true; }
    if (s == "false" || s == "no" || s == "0") { out = false; return true; }
    return false;
}

bool parseFloat(const std::string& s, float& out) {
    try {
        size_t n = 0;
        out = std::stof(s, &n);
        return n == s.size();
    } catch (...) {
        return false;
    }
}

bool parseInt(const std::string& s, int& out) {
    try {
        size_t n = 0;
        out = std::stoi(s, &n);
        return n == s.size();
    } catch (...) {
        return false;
    }
}

// "a,b" -> two floats.
bool parsePair(const std::string& s, float& a, float& b) {
    const size_t comma = s.find(',');
    if (comma == std::string::npos) return false;
    return parseFloat(trim(s.substr(0, comma)), a) &&
           parseFloat(trim(s.substr(comma + 1)), b);
}

bool parseEmitter(const std::string& s, FxEmitter& out) {
    if (s == "point") { out = FxEmitter::Point; return true; }
    if (s == "cone") { out = FxEmitter::Cone; return true; }
    if (s == "ring") { out = FxEmitter::Ring; return true; }
    if (s == "sphere") { out = FxEmitter::Sphere; return true; }
    if (s == "beam") { out = FxEmitter::Beam; return true; }
    if (s == "wall") { out = FxEmitter::Wall; return true; }
    return false;
}

bool isHexColor(const std::string& s) {
    if (s.size() != 6) return false;
    for (char c : s) {
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                         (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

} // namespace

const FxPreset* FxLibrary::find(const std::string& id) const {
    for (const auto& p : presets)
        if (p.id == id) return &p;
    return nullptr;
}

FxLoadResult parseFxLibraryText(const std::string& text) {
    FxLoadResult r;
    FxPreset cur;
    bool inPreset = false;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;

    auto finish = [&]() -> bool {
        if (!inPreset) return true;
        if (cur.id.empty()) {
            r.error = "preset with empty id";
            return false;
        }
        if (r.library.find(cur.id) != nullptr) {
            r.error = "duplicate preset '" + cur.id + "'";
            return false;
        }
        // Sanity: ranges ordered, colors valid.
        if (cur.lifetimeMin > cur.lifetimeMax || cur.speedMin > cur.speedMax ||
            cur.sizeMin > cur.sizeMax) {
            r.error = "preset '" + cur.id + "': range min > max";
            return false;
        }
        if (!isHexColor(cur.color0) || !isHexColor(cur.color1)) {
            r.error = "preset '" + cur.id + "': bad hex color";
            return false;
        }
        if (cur.count <= 0) {
            r.error = "preset '" + cur.id + "': count must be positive";
            return false;
        }
        r.library.presets.push_back(cur);
        return true;
    };

    while (std::getline(in, line)) {
        ++lineNo;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            if (!finish()) return r;
            cur = FxPreset{};
            cur.id = trim(line.substr(1, line.size() - 2));
            inPreset = true;
            continue;
        }
        if (!inPreset) {
            r.error = "line " + std::to_string(lineNo) +
                      ": key before first [preset] section";
            return r;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            r.error = "line " + std::to_string(lineNo) + ": no '='";
            return r;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));

        float f = 0.0f, g = 0.0f;
        int n = 0;
        bool b = false;
        FxEmitter e = FxEmitter::Point;

        if (key == "display_name") {
            cur.displayName = val;
        } else if (key == "emitter" && parseEmitter(val, e)) {
            cur.emitter = e;
        } else if (key == "emitter") {
            r.error = "line " + std::to_string(lineNo) +
                      ": unknown emitter '" + val + "'";
            return r;
        } else if (key == "count" && parseInt(val, n)) {
            cur.count = n;
        } else if (key == "lifetime" && parsePair(val, f, g)) {
            cur.lifetimeMin = f;
            cur.lifetimeMax = g;
        } else if (key == "speed" && parsePair(val, f, g)) {
            cur.speedMin = f;
            cur.speedMax = g;
        } else if (key == "spread" && parseFloat(val, f)) {
            cur.spreadDeg = f;
        } else if (key == "size" && parsePair(val, f, g)) {
            cur.sizeMin = f;
            cur.sizeMax = g;
        } else if (key == "colors") {
            const size_t comma = val.find(',');
            if (comma == std::string::npos) {
                r.error = "line " + std::to_string(lineNo) +
                          ": colors needs 'hex0,hex1'";
                return r;
            }
            cur.color0 = trim(val.substr(0, comma));
            cur.color1 = trim(val.substr(comma + 1));
        } else if (key == "additive" && parseBool(val, b)) {
            cur.additive = b;
        } else if (key == "gravity" && parseFloat(val, f)) {
            cur.gravity = f;
        } else if (key == "texture") {
            cur.texture = val;
        } else if (key == "duration" && parseFloat(val, f)) {
            cur.durationSec = f;
        } else if (key == "loop" && parseBool(val, b)) {
            cur.loop = b;
        } else {
            r.error = "line " + std::to_string(lineNo) + ": bad key '" +
                      key + "' or value '" + val + "'";
            return r;
        }
    }
    if (!finish()) return r;
    r.ok = true;
    return r;
}

FxLoadResult loadFxLibrary(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        FxLoadResult r;
        r.error = "cannot open fx library '" + path + "'";
        return r;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parseFxLibraryText(ss.str());
}

const FxPreset* fxForSpell(const SpellDef& spell, const FxLibrary& lib) {
    if (!spell.fxPreset.empty()) return lib.find(spell.fxPreset);
    return lib.find(spell.effectKind);
}

const FxPreset* fxForHeavyAttack(const HeavyAttackDef& heavy,
                                 const FxLibrary& lib) {
    if (!heavy.fxPreset.empty()) return lib.find(heavy.fxPreset);
    return lib.find(heavyAttackKindName(heavy.kind));
}

namespace {

// Case-insensitive substring test, for rock break-event prose ("the
// rock shatters", "the rock crumbles to dust", ...). The rip/hurl
// moments match exact tags instead; this catches break prose without
// hard-coding every flavor string MatchKits.cpp invents.
bool tagMentionsRock(const std::string& tag) {
    std::string lower = tag;
    for (char& c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower.find("rock") != std::string::npos;
}

} // namespace

const FxPreset* fxForEvent(EventType type, const FxLibrary& lib) {
    switch (type) {
        case EventType::CityBuildingDestroyed:
        case EventType::DistrictRazed:
        case EventType::CityDestroyed:
            return lib.find("raze");
        case EventType::PlayerLeveledUp:
            // 2 PM build (2026-10-12): level-ups reuse the buff preset —
            // no new FX asset needed, still within the particle budget.
            return lib.find("buff");
        case EventType::TowerDestroyed:
            // Micro-dev (2026-10-12): a fallen tower is a razed structure —
            // reuse the raze preset, no new FX asset needed.
            return lib.find("raze");
        case EventType::GreatOldOneSlain:
            // 5 PM beauty (2026-10-12): a god falling is as heavy as a
            // city — the heaviest burst in the catalog. No new preset.
            return lib.find("raze");
        case EventType::GreatOldOneUnderAttack:
        case EventType::TowerUnderAttack:
        case EventType::PlayerLowHp:
            // Micro-dev (2026-10-12): an alarm burst at the god's position
            // — the same sharp cue as kill streaks. The low-health ping
            // (fired once per 35%-HP crossing) was text-only until now;
            // it resolves here too, so the engine gets a danger cue at
            // the fighter's position. No new preset.
            // Micro-dev (2026-10-13): a tower's first scratch is the same
            // alarm class — it resolves here too, no new preset.
            return lib.find("stun");
        // 5 PM beauty (2026-10-12): the announced-events FX sweep. These
        // are the game's punctuation marks — kills, streaks, streak-
        // breakers, manifestations, captures, triumphs — and they were
        // text-only. All reuse existing presets, so the particle payload
        // and budget are untouched; the engine just gets a visual cue to
        // play for the same events the console announces.
        case EventType::KillStreak:
        case EventType::StreakEnded:
        case EventType::CCApplied:
        case EventType::OvertimeStarted:
        case EventType::RelicChannelInterrupted:
        case EventType::SummoningInterrupted:
            // A sharp burst for the match's punctuation moments:
            // DOUBLE KILL / RAMPAGE, the shutdown callout, the held-fast
            // jolt, and the sudden-death jolt. Micro-dev (2026-10-13):
            // a stun/root landing on anyone is now visible to the engine
            // at the victim's position — the console alert was text-only
            // until now.
            //
            // Micro-dev (2026-10-13): the relic-channel-break beat lands
            // here too — a snapped conjuration reads as a jolt at the
            // channel site, the anti-image of the channel-start halo.
            //
            // Micro-dev (2026-10-13): a snapped GrandSummoning rite lands
            // here as well — the anti-image of the ChampionSummoned
            // arrival halo, a jolt at the rite site where the summoning
            // broke (both publish sites stamp pos).
            // No new preset needed.
            return lib.find("stun");
        case EventType::FirstBlood:
        case EventType::PlayerKilled:
        case EventType::DholeAmbush:
        case EventType::MonstrositySlain:
        case EventType::CivilianSlain:
            // Raw violence — impact smoke at the scene of the death.
            // Micro-dev (2026-10-13): a slain monstrosity joins the
            // death beats — same impact burst, no new preset.
            //
            // Micro-dev (2026-10-13): civilians slain by abilities join
            // the death beats too — ability kills (KitCaster), Wave of
            // Domination victims, and razed-city casualties resolved to
            // nullptr while fighter and monstrosity deaths had impact
            // smoke. Same burst, no new preset.
            return lib.find("aoe_damage");
        case EventType::RelicManifested:
        case EventType::RelicClaimed:
        case EventType::MatchStarted:
        case EventType::ChampionSummoned:
            // A rift opens as the relic manifests / is seized — and as
            // the arena itself opens. Micro-dev (2026-10-13): the
            // match's opening beat had no visual; it reads as a
            // manifestation, same preset class, no new asset.
            //
            // Micro-dev (2026-10-13): a champion answering the
            // GrandSummoning rite is the same arrival class — a
            // manifestation at the summoning site. No new preset.
            return lib.find("summon");
        // Micro-dev (2026-10-13): the Wave of Domination's lifecycle
        // beats were FX-less — in DOTGOO, Cthulhu's signature RMB
        // (mind-control wave, levitate, slam, launch) reached the event
        // pipeline but never resolved to a visual, while the Domination
        // rock kit has full coverage. All reuse existing presets — no
        // new FX asset, particle budget untouched.
        case EventType::DirectStunApplied:
            // The targeted gaze stun — a sharp jolt at the victim.
            return lib.find("stun");
        case EventType::VictimLevitated:
            // The wave's own moment: a victim rising in the
            // dominion-wave mist. The MindControl preset was authored
            // for exactly this.
            return lib.find("MindControl");
        case EventType::VictimSlammed:
        case EventType::VictimsLaunched:
            // Bodies slammed into buildings / flung as projectiles —
            // impact smoke at the scene.
            return lib.find("aoe_damage");
        case EventType::MaddeningWhispers:
            // Dread mist rolling through the Vale — the fear_aura preset
            // was practically authored for this event.
            return lib.find("fear_aura");
        // Micro-dev (2026-10-12): the dungeon-hazard sweep. These fired
        // from DungeonInstance/ValeOfPnath with no preset at all, so the
        // engine got no visual cue for spike pits, cave-ins, abyss-pit
        // falls, or the dhole's tremor telegraph. All reuse existing
        // presets — no new FX asset, particle budget untouched.
        case EventType::SpikePitSprung:
        case EventType::CaveIn:
        case EventType::AbyssPitFall:
            // Impact smoke at the hazard site.
            return lib.find("aoe_damage");
        case EventType::DholeTremors:
            // The telegraph warning: dread mist rolling through the Vale,
            // same preset as the whispers it keeps company with.
            return lib.find("fear_aura");
        case EventType::PointCaptured:
        case EventType::AchievementUnlocked:
        case EventType::MatchPoint:
        case EventType::MatchEnded:
        case EventType::PlayerRespawned:
        case EventType::DiscoveryMade:
        case EventType::DungeonCompleted:
        case EventType::UltCharged:
        case EventType::RelicChannelStarted:
        case EventType::AbilityUnlocked:
            // The golden halo of triumph and return: captures, records,
            // match point, the match's own end, respawns, first
            // discoveries, dungeon clears — and the ult-ready ping.
            // Micro-dev (2026-10-12): UltCharged
            // shipped after the 5 PM sweep; folded in here per its note,
            // reusing buff so no new preset is needed.
            //
            // Micro-dev (2026-10-13): the relic-channel-start beat lands
            // here too — a fighter committing to a 4s/6s channel deserves
            // the same halo the claims get.
            //
            // Micro-dev (2026-10-13): the DOTGOO draft-pick beat (Q/F at
            // levels 1/3, the level-6 ult) lands here too — same class as
            // the ult-ready ping, a golden halo at the fighter's pos
            // (stamped at all three publish sites) when the pick lands.
            return lib.find("buff");
        default:
            return nullptr;
    }
}

const FxPreset* fxForEvent(const GameEvent& e, const FxLibrary& lib) {
    if (e.type == EventType::AbilityCast && e.amount == 3.0f) {
        // RMB slot. Rock-kit moments first; ordinary heavy attacks
        // resolve through fxForHeavyAttack with the caster's kit.
        if (e.tag == kRockRipTag) return lib.find("rock_rip");
        if (e.tag == kRockHurlTag) return lib.find("rock_hurl");
        // Micro-dev (2026-10-13): the exact swing tag goes before the
        // break-prose substring test — "rock swing" mentions rock, and
        // a swing is a cleave, not a shatter.
        if (e.tag == kRockSwingTag) return lib.find("MeleeHeavy");
        if (tagMentionsRock(e.tag)) return lib.find("rock_shatter");
        return nullptr;
    }
    return fxForEvent(e.type, lib);
}

} // namespace cultulhu
