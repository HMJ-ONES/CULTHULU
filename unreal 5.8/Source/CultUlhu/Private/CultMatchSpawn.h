#pragma once
#include "CultUlhuGameInstance.h"

namespace CultMatchSpawn {
// Initial possession and later respawns must agree before the first physics tick.
inline FVector Feet(ECultPlayMode Mode,int32 Team,int32 TeamSlot=0) {
 const float Extent=Mode==ECultPlayMode::Dotgoo?30000.f:23000.f;
 const float Forward=Team==0?1.f:-1.f;
 return FVector(-Forward*Extent+Forward*2600,Forward*(1-FMath::Clamp(TeamSlot,0,4))*1500,0);
}
}
