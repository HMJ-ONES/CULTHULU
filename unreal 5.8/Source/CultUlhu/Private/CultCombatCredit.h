#pragma once
#include "CultUlhuPlayerPawn.h"
#include "CultPopulation.h"

namespace CultCombatCredit {
// Credit the explicit summoned creature, never an arbitrary actor ownership chain.
// Keep the original source actor for collision, faction checks and incoming direction.
inline ACultUlhuPlayerPawn* Hero(AActor* Source) {
 if(!IsValid(Source)) return nullptr;
 if(auto* Player=Cast<ACultUlhuPlayerPawn>(Source)) return Player;
 if(auto* Creature=Cast<ACultPopulation>(Source)) if(Creature->IsSummoned()) {
  auto* Owner=Cast<ACultUlhuPlayerPawn>(Creature->GetOwner());
  if(IsValid(Owner)&&Owner->GetWorld()==Creature->GetWorld()&&Owner->GetFaction()==Creature->GetFaction()) return Owner;
 }
 return nullptr;
}
}
