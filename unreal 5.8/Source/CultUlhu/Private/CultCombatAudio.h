#pragma once
#include "CoreMinimal.h"
class UWorld;
enum class ECultCombatCue : uint8 { Hit,Kill,Dash,Evade,Hurt };
void PlayCultCombatCue(UWorld* World,ECultCombatCue Kind,float Volume=.24f);
