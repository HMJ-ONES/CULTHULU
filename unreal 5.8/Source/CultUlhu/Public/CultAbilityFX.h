#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CultAbilityFX.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class ACultUlhuPlayerPawn;

namespace CultAbilityFX {
enum class ELook : uint8 {
 None, Abyss, Tendrils, Dread, Tide, Ember, Obsidian, Sigil, Nerve,
 Stasis, Spore, Mend, Blade, Venom, Prism, Gate, Brood, Chitin, Stone, Count
};
enum class EEvent : uint8 { Release, Contact, Arrival, Recovery, Count };

ELook ForSlot(const FString& Id,int32 Slot);
void CastCue(ACultUlhuPlayerPawn* Pawn,const FString& Id,uint8 Move,float Windup,float Age);
}

UCLASS(Transient)
class CULTULHU_API ACultAbilityFX : public AActor {
 GENERATED_BODY()
public:
 ACultAbilityFX();
 static ACultAbilityFX* Spawn(UWorld* World,CultAbilityFX::ELook Look,CultAbilityFX::EEvent Event,
  FVector Point,FVector Direction,float Size,ACultUlhuPlayerPawn* Anchor=nullptr,float Charge=0,float Age=0,
  bool TrailCaster=false);
 void Tick(float DeltaSeconds) override;

private:
 void Draw();
 UPROPERTY() TObjectPtr<UProceduralMeshComponent> Filaments;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
 TWeakObjectPtr<ACultUlhuPlayerPawn> Follow;
 TArray<FVector> MotionWake;
 CultAbilityFX::ELook Look=CultAbilityFX::ELook::None;
 CultAbilityFX::EEvent Event=CultAbilityFX::EEvent::Release;
 FVector Axis=FVector::ForwardVector;
 float Scale=100,Age=0,Windup=0,Life=.63f;
 bool bFollowsCaster=false,bTrailCaster=false;
};
