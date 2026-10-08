#pragma once

// Wave 4: the action -> exertion table. Every gameplay action feeds the
// fervor/exertion meters of one or more beliefs. All amounts live HERE as
// data (creative-liberty tuning) instead of being scattered as magic
// numbers through the systems. ExertionSystem consumes this table.

#include "beliefs/Belief.h"
#include "commands/CommandSystem.h"
#include "core/Events.h"

#include <cstddef>

namespace cultulhu {

struct ExertionFeed {
    EventType type;
    Belief belief;
    float amount; // added to the belief's exertion (may be negative)
};

// One row per (event, belief) pair. A single event can feed several beliefs.
inline constexpr ExertionFeed ACTION_EXERTION_TABLE[] = {
    // Torture
    {EventType::TorturePerformed,   Belief::Torture,   8.0f},
    {EventType::TorturePerformed,   Belief::Fear,      3.0f},
    {EventType::CapturedTortured,   Belief::Torture,   5.0f},
    {EventType::CapturedTortured,   Belief::Fear,      2.0f},
    // Fear
    {EventType::RaidPerformed,      Belief::Fear,     10.0f},
    {EventType::CityBuildingDestroyed, Belief::Fear,   6.0f},
    {EventType::DistrictRazed,      Belief::Fear,     10.0f},
    // Breeding
    {EventType::MonstrosityBred,    Belief::Breeding,  8.0f},
    {EventType::FeralRampage,       Belief::Breeding,  4.0f},
    {EventType::FeralRampage,       Belief::Fear,      5.0f},
    // Chaos
    {EventType::Explosion,          Belief::Chaos,     6.0f},
    {EventType::InfringerPunished,  Belief::Chaos,     4.0f},
    {EventType::BrawlBrokeOut,      Belief::Chaos,     3.0f},
    {EventType::DesecrationDone,    Belief::Chaos,     2.0f},
    {EventType::LunaticActed,       Belief::Chaos,     3.0f},
    // Sacrifice (wave 10 / R2: 12.0 every ~300 ticks = 0.04/s could never
    // beat the 0.05/s decay, leaving Dark Rites / Martyrs' Visions dead)
    {EventType::SacrificeCompleted, Belief::Sacrifice, 20.0f},
    {EventType::SacrificeInterrupted, Belief::Sacrifice, -5.0f},
    // Conversion
    {EventType::ConversionPerformed, Belief::Conversion, 6.0f},
    {EventType::MassConversionUsed, Belief::Conversion, 10.0f},
    {EventType::SermonPreached,     Belief::Conversion, 2.0f},
    // War
    {EventType::EnemyCultistSlain,  Belief::War,       6.0f},
    // Reconstruction
    // Reconstruction (wave 10 / R2: 2.0/200 ticks + 1.0/60 ticks = 0.027/s
    // could never beat the 0.05/s decay, so Reconstruction never warmed)
    {EventType::BuildingRebuilt,    Belief::Reconstruction, 8.0f},
    {EventType::HealPerformed,      Belief::Reconstruction, 2.0f},
    // Trickery
    {EventType::MimicKill,          Belief::Trickery,  8.0f},
    {EventType::TrapSprung,         Belief::Trickery,  6.0f},
    {EventType::ArtifactTriggered,  Belief::Trickery,  4.0f},
    // Magic
    {EventType::SpellCast,          Belief::Magic,     3.0f},
    {EventType::NecromancyPerformed, Belief::Magic,   10.0f},
    // Onslaught
    {EventType::CivilianSlain,      Belief::Onslaught, 3.0f},
    {EventType::MeleeAttack,        Belief::Onslaught, 1.0f},
    // Dreams (wave 10 / R2: 1.0/150 ticks + rests/nightmares = 0.021/s could
    // never beat the 0.05/s decay, leaving Nightmare Surge / Oneiromancy /
    // Martyrs' Visions dead)
    {EventType::RestStarted,        Belief::Dreams,    2.0f},
    {EventType::DreamWhisper,       Belief::Dreams,    8.0f},
    {EventType::DreamWhisper,       Belief::Conversion, 1.0f},
    {EventType::PrayerOffered,       Belief::Dreams,    1.0f},
    {EventType::Nightmare,          Belief::Dreams,    3.0f},
    {EventType::Nightmare,          Belief::Chaos,     2.0f},
    // Wave 9c: new ambient activities.
    {EventType::OmenRead,           Belief::Dreams,    3.0f},
    {EventType::SparringHeld,       Belief::War,       2.0f},
    {EventType::SparringHeld,       Belief::Onslaught, 1.0f},
    {EventType::SigilPainted,       Belief::Fear,      2.0f},
    {EventType::ChantingHeld,       Belief::Magic,     3.0f},
    // Wave 9c: dungeon hazards.
    {EventType::SpikePitSprung,     Belief::Fear,      2.0f},
    {EventType::CaveIn,             Belief::Chaos,     4.0f},
    // (Dungeon traps reuse TrapSprung -> Trickery 6.0f, already above.)
    // Wave 13: Vale of Pnath hazards & encounters.
    {EventType::AbyssPitFall,       Belief::Fear,      6.0f},
    {EventType::MaddeningWhispers,  Belief::Fear,      4.0f},
    {EventType::DholeTremors,       Belief::Fear,      2.0f},
    {EventType::DholeAmbush,        Belief::Fear,      8.0f},
    {EventType::ValeRelicClaimed,   Belief::Magic,     8.0f},
    // Wave 9b: new directives.
    {EventType::LeaderAssassinated, Belief::Fear,     15.0f},
    {EventType::LeaderAssassinated, Belief::War,       6.0f},
    {EventType::EnemyMoraleShocked, Belief::Conversion, 5.0f},
    {EventType::AssassinExposed,    Belief::Fear,       3.0f},
    {EventType::ZoneBlightTick,     Belief::Fear,       1.0f},
    {EventType::ZoneBlighted,       Belief::Fear,      10.0f},
    {EventType::SummoningInterrupted, Belief::Magic,   -5.0f},
    {EventType::ChampionSummoned,   Belief::Magic,     10.0f},
    {EventType::ChampionSummoned,   Belief::Fear,       5.0f},
};

inline constexpr std::size_t ACTION_EXERTION_TABLE_SIZE =
    sizeof(ACTION_EXERTION_TABLE) / sizeof(ACTION_EXERTION_TABLE[0]);

// Eldritch directives feed exertion too, on resolution. Obeyed (or partially
// obeyed) directives feed their aligned belief; refused directives feed
// Chaos instead — defiance is its own kind of worship.
struct DirectiveFeed {
    DirectiveType directive;
    Belief belief;     // belief fed when the directive is obeyed
    float obeyed;      // exertion on Obeyed
    float refused;     // exertion to Chaos on Refused
};

inline constexpr DirectiveFeed DIRECTIVE_EXERTION_TABLE[] = {
    {DirectiveType::RaidCity,       Belief::Fear,           8.0f, 4.0f},
    {DirectiveType::ConvertCampaign, Belief::Conversion,    8.0f, 4.0f},
    {DirectiveType::MassSacrifice,  Belief::Sacrifice,      8.0f, 4.0f},
    {DirectiveType::GoToWar,        Belief::War,            8.0f, 4.0f},
    {DirectiveType::Defend,         Belief::Reconstruction, 6.0f, 3.0f},
    {DirectiveType::GatherRelic,    Belief::Magic,          6.0f, 3.0f},
    // Wave 9b: new directives.
    {DirectiveType::AssassinateProphet, Belief::Trickery,   8.0f, 4.0f},
    {DirectiveType::BlightLand,    Belief::Fear,            8.0f, 4.0f},
    {DirectiveType::GrandSummoning, Belief::Magic,         10.0f, 5.0f},
};

inline constexpr std::size_t DIRECTIVE_EXERTION_TABLE_SIZE =
    sizeof(DIRECTIVE_EXERTION_TABLE) / sizeof(DIRECTIVE_EXERTION_TABLE[0]);

} // namespace cultulhu
