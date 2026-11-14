#pragma once

#include "CultMatchDirector.h"
#include "CultUlhuPlayerPawn.h"

namespace CultBotFocus {
inline int32 AlliesAimingAt(const ACultMatchDirector& Match,const ACultUlhuPlayerPawn* Bot,const AActor* Target) {
 int32 Count=0;
 for(const auto& Ally:Match.Fighters()) {
  const auto* Pawn=Ally.Pawn.Get();
  if(Ally.Team==Bot->GetFaction()&&Ally.Respawn<=0&&Pawn&&Pawn!=Bot&&Pawn->IsBot()&&
     !Ally.Retreating&&!Pawn->IsIncapacitated()&&Pawn->GetBotAim()==Target) ++Count;
 }
 return Count;
}
}
