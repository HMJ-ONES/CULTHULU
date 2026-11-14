#pragma once
#include "CoreMinimal.h"
class UWorld;
class ASkyLight;
namespace CultSceneLighting {
 // Presentation is local to each rendering world. GameMode exists only on the
 // server, so its setup cannot be the only source of guest ambient lighting.
 ASkyLight* Ambient(UWorld* World,float Intensity,FLinearColor Color);
}
