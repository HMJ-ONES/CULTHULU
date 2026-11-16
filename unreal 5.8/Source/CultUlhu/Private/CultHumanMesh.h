#pragma once
#include "CoreMinimal.h"
class USkeletalMesh;

namespace CultHumanMesh {
bool Enabled();
USkeletalMesh* Find(const FString& Skin);
int32 SourceParts(const USkeletalMesh* Mesh);
bool Verify(const FString& Skin,FString& Details);
void Reset();
}
