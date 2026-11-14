#pragma once
#include "CoreMinimal.h"
class ACultSkillAttack;
class ACultUlhuPlayerPawn;
struct FCultBotThreatMemory {
 TWeakObjectPtr<ACultSkillAttack> Seen;
 float Since=0,NextObservation=0,EscapeUntil=0;
 FVector Escape=FVector::ZeroVector;
};
namespace CultBotThreat {
 bool Respond(ACultUlhuPlayerPawn* Bot,FCultBotThreatMemory& Memory,float Now,int Side);
 bool Avoiding(const FCultBotThreatMemory& Memory,float Now);
 bool SafeDash(ACultUlhuPlayerPawn* Bot,FVector Direction);
}
