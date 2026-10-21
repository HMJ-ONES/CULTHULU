#pragma once
#include "CoreMinimal.h"
#include "animation/AnimationClip.h"
namespace CultHumanAnimation {
 FString Path(const FString& Skin,const FString& Clip);
 const cultulhu::AnimationClip& Find(const FString& Skin,const FString& Clip);
}
