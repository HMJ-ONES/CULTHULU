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
    // Sacrifice
    {EventType::SacrificeCompleted, Belief::Sacrifice, 12.0f},
    {EventType::SacrificeInterrupted, Belief::Sacrifice, -5.0f},
    // Conversion
    {EventType::ConversionPerformed, Belief::Conversion, 6.0f},
    {EventType::MassConversionUsed, Belief::Conversion, 10.0f},
    {EventType::SermonPreached,     Belief::Conversion, 2.0f},
    // War
    {EventType::EnemyCultistSlain,  Belief::War,       6.0f},
    // Reconstruction
    {EventType::BuildingRebuilt,    Belief::Reconstruction, 2.0f},
    {EventType::HealPerformed,      Belief::Reconstruction, 1.0f},
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
    // Dreams
    {EventType::RestStarted,        Belief::Dreams,    2.0f},
    {EventType::DreamWhisper,       Belief::Dreams,    1.0f},
    {EventType::DreamWhisper,       Belief::Conversion, 1.0f},
    {EventType::PrayerOffered,       Belief::Dreams,    1.0f},
    {EventType::Nightmare,          Belief::Dreams,    3.0f},
    {EventType::Nightmare,          Belief::Chaos,     2.0f},
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
};

inline constexpr std::size_t DIRECTIVE_EXERTION_TABLE_SIZE =
    sizeof(DIRECTIVE_EXERTION_TABLE) / sizeof(DIRECTIVE_EXERTION_TABLE[0]);

} // namespace cultulhu
