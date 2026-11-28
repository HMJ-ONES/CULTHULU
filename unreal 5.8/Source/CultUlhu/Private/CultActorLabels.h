#pragma once
#include "GameFramework/Actor.h"

// Debug labels are editor data. Gameplay anchors use serialized runtime tags.
namespace CultActorLabels {
inline void Set(AActor* Actor,const FString& Label)
{
#if WITH_EDITOR
 if(Actor) Actor->SetActorLabel(Label);
#endif
}
inline bool Anchor(const AActor* Actor,FName Tag)
{
 if(!Actor) return false;
 if(Actor->ActorHasTag(Tag)) return true;
#if WITH_EDITOR
 return Actor->GetActorLabel()==Tag.ToString(); // Existing authoring maps before tag migration.
#else
 return false;
#endif
}
}
