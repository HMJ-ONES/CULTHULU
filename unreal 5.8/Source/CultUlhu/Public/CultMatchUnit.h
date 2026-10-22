#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CultControlProjection.h"
#include "CultMatchUnit.generated.h"
class ACultUlhuPlayerPawn;
class ACultSkillAttack;
UCLASS()
class CULTULHU_API ACultMatchUnit:public AActor {
 GENERATED_BODY()
public:
 ACultMatchUnit();
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 // 0 melee, 1 ranged, 2 siege, 3 lane tower, 4 Great Old One, 5 nexus ward.
 int Team=0,Lane=1,Kind=0,Index=-1,Waypoint=0;
 float Health=120,MaxHealth=120,Dps=8,Range=800,Speed=600,AttackCooldown=0,Life=0;
 TWeakObjectPtr<AActor> AttackTarget;
 bool IsInTransit() const { return Transit!=ETransit::None; }
 bool IsHeldBy(const ACultUlhuPlayerPawn* Actor) const;
 bool TryLift(class ACultUlhuPlayerPawn* Caster);
 void StartFlight(class ACultSkillAttack* Projectile);
 void LandMiss(FVector Point);
 void ConsumeThrown(class ACultUlhuPlayerPawn* Source);
 bool RejoiningLane=false;
 FVector RejoinPoint=FVector::ZeroVector;
 TArray<FVector> RejoinPath;
 float GetHealth() const { return Health; }
 float GetMaxHealth() const { return MaxHealth; }
 int GetFaction() const { return Team; }
 bool IsStructure() const { return Kind>=3; }
 void Hurt(float Amount,AActor* Source);
 void Heal(float Amount) { if(Health>0) Health=FMath::Min(MaxHealth,Health+Amount); }
 void Die();
 void FireAt(AActor* Target);
 void MoveTo(FVector Point,float Dt);
 void SetControlState(bool Stop,bool Fear,float SpeedScale,FVector Origin,bool Stun=false,int32 Source=0) { Control.Set(Source,Stop,Fear,SpeedScale,Origin,Stun); }
 bool Stopped() const { return Control.Stop||Control.Fear; }
 FString Title() const;
private:
 UPROPERTY() class UCapsuleComponent* Capsule=nullptr;
 UPROPERTY() TArray<class UStaticMeshComponent*> Parts;
 enum class ETransit { None,Held,Thrown };
 ETransit Transit=ETransit::None;
 TWeakObjectPtr<class ACultUlhuPlayerPawn> Holder;
 TWeakObjectPtr<class ACultSkillAttack> Flight;
 FVector Pickup=FVector::ZeroVector;
 float TransitTime=0;
 bool UpdateTransit(float Dt);
 FCultControlProjection Control;
 float Phase=0;
 FVector Previous=FVector::ZeroVector;
 void CreateArtBody();
 void AnimateArtBody(float Dt,bool Walking);
 UPROPERTY() class UPoseableMeshComponent* ArtBody=nullptr;
 UPROPERTY() TArray<class UPoseableMeshComponent*> ArtParts;
 TArray<FTransform> RestPose;
 FString Skin;
 float ArtTime=0;
};
