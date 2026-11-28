#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CultChaosShield.generated.h"

UCLASS()
class ACultChaosShield : public AActor {
 GENERATED_BODY()
public:
 static constexpr float HalfWidth=1900.f;
 static constexpr float Height=3250.f;
 static constexpr float Lifetime=10.f;
 ACultChaosShield();
 void Initialize(int InFaction,float Integrity);
 void Absorb(float Amount);
 float Durability() const { return Remaining; }
 static bool FirstHostile(UWorld* World,AActor* Source,FVector Start,FVector End,
  const FCollisionShape& Shape,FQuat Rotation,FHitResult& Hit);
 void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
 void BeginPlay() override;
private:
 UPROPERTY() TObjectPtr<class UBoxComponent> Collision;
 UPROPERTY() TObjectPtr<class UProceduralMeshComponent> Surface;
 UPROPERTY(Replicated) int Faction=INDEX_NONE;
 UPROPERTY(ReplicatedUsing=UpdateAppearance) float Remaining=1.f;
 UPROPERTY(Replicated) float Capacity=1.f;
 UFUNCTION() void UpdateAppearance();
};
