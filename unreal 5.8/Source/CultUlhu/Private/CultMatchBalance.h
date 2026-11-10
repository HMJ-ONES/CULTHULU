#pragma once

#include "CultUlhuGameInstance.h"

namespace CultMatchBalance {
inline constexpr float CompetitiveBaseHealth=100.f;
inline constexpr float DominationHealth=135.f;
inline constexpr float LaneTowerHealth=2800.f;
inline float HeroHealth(ECultPlayMode Mode,float FreeRoamHealth) {
 return Mode==ECultPlayMode::FreeRoam?FreeRoamHealth:Mode==ECultPlayMode::Domination?DominationHealth:CompetitiveBaseHealth;
}
}
