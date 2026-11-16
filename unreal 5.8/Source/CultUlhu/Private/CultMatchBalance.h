#pragma once

#include "CultUlhuGameInstance.h"

namespace CultMatchBalance {
inline constexpr float CompetitiveBaseHealth=100.f;
inline constexpr float DominationHealth=135.f;
inline constexpr float LaneTowerHealth=2800.f;
inline constexpr int HeroAssistXP=25;
inline constexpr int WardLastHitXP=180;
inline constexpr int WardTeamXP=80;
inline constexpr int MinionLastHitXP=30;
inline constexpr int MinionNearbyXP=18;
inline constexpr float MinionXPRadius=7800.f;
inline int HeroKillXP(int KillerLevel,int VictimLevel) {
 return 65+5*FMath::Clamp(VictimLevel-KillerLevel,0,6);
}
inline float HeroHealth(ECultPlayMode Mode,float FreeRoamHealth) {
 return Mode==ECultPlayMode::FreeRoam?FreeRoamHealth:Mode==ECultPlayMode::Domination?DominationHealth:CompetitiveBaseHealth;
}
}
