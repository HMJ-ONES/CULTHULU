// CULT-ULHU feral beast: simple live AI for the Dark Wilds.
//
// Spawned by unreal/Tools/BuildDarkWilds.py from the `spawn` entries in
// dark_wilds.map (species, position, count, zone), replacing the old static
// display meshes. Imported modular skeletal pieces share a procedurally
// animated pose; capsule collision and ground sampling drive movement.
// Minor enemies do 5–10 damage and fall to one normal ground smash.
#pragma once

#include "CoreMinimal.h"
#include "CultControlProjection.h"
#include "GameFramework/Actor.h"

#include "CultFeralBeast.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class CULTULHU_API ACultFeralBeast : public AActor
{
	GENERATED_BODY()

public:
	ACultFeralBeast();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Called by the build script right after spawning.
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Beast")
	void SetMesh(UStaticMesh* InMesh);

	// Species id from the .map spawn entry (stats table in the .cpp).
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Beast")
	void SetSpecies(const FString& SpeciesId);

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Beast")
	void ApplyDamage(float Amount);
 void ReceiveCombatDamage(float Amount,AActor* Source);
 bool IsJungleMonster() const { return JungleCamp_.IsValid(); }

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Beast")
	bool IsDead() const { return State_ == EState::Dead || State_ == EState::Dying; }
	float GetHealth() const { return Health_; }
 float RecoveryRemaining() const { return AttackAnimTime_>0?0.f:RecoveryTime_; }
 float StaggerRemaining() const { return StaggerTime_; }
 float GetMaxHealth() const { return MaxHealth_; }
 FString Species() const { return SpeciesId_; }
 void Capture(AActor* Escort=nullptr,int32 Faction=-1) { if(IsJungleMonster()) return; bCaptured_=true; Captor_=Escort; CaptiveFaction_=Faction; }
 void ReleaseCaptive() { bCaptured_=false; Captor_.Reset(); CaptiveFaction_=-1; }
 int32 GetCaptiveFaction() const { return CaptiveFaction_; }
 bool IsCaptured() const { return bCaptured_; }
 void SetBredHealth(float Hp) { MaxHealth_=Health_=Hp; }
	float GetAttackDamage() const { return AttackDamage_; }
	int32 GetCreaturePartCount() const { return CreatureParts_.Num(); }
	bool HasCreatureMesh() const { return CreaturePose_ != nullptr; }
	void SetControlState(bool Stop,bool Fear,float Speed,FVector Origin,bool Stun=false,int32 Source=0) { Control_.Set(Source,Stop,Fear,Speed,Origin,Stun); }

protected:
	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Beast")
	UStaticMeshComponent* MeshComp_ = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Beast")
	class UCapsuleComponent* Collision_ = nullptr;
	UPROPERTY(Transient)
	class UPoseableMeshComponent* CreaturePose_ = nullptr;
	UPROPERTY(Transient)
	TArray<class UPoseableMeshComponent*> CreatureParts_;

	// Glowing predator eyes (HDR material, positioned from mesh bounds).
	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Beast")
	UStaticMeshComponent* EyeL_ = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Beast")
	UStaticMeshComponent* EyeR_ = nullptr;

private:
 friend class ACultJungleCamp;
 TWeakObjectPtr<class ACultJungleCamp> JungleCamp_;
 friend struct FCultCombatFeelTestAccess;
	enum class EState : uint8 { Wander, Chase, Dying, Dead };
	EState State_ = EState::Wander;

	void ApplySpeciesStats();
	void PickWanderTarget();
	void AttachEyes();
	void AssembleCreature();
	void AnimateCreature(float DeltaSeconds, float DistanceMoved);
	bool GroundAt(FVector Position, FVector& OutPosition) const;
	void MoveToward(const FVector& Dest, float SpeedMs, float DeltaSeconds);
	void FaceToward(const FVector& Dest, float DeltaSeconds, float Rate = 6.0f);
	void Die();
 void BeginSwipe(FVector Target);

	UPROPERTY(EditAnywhere, Category = "CultUlhu|Beast")
	FString SpeciesId_ = TEXT("skittering_ghoul");
	float Health_ = 60.0f;
 bool bCaptured_=false;
 int32 CaptiveFaction_=-1;
 TWeakObjectPtr<AActor> Captor_;
	float MaxHealth_ = 60.0f;
	float Speed_ = 5.0f;   // m/s
	float BodyScale_ = 1.0f;
	float AttackDamage_ = 6.f;
	float GaitPhase_ = 0.f;
	float AttackAnimTime_ = 0.f;
 float RecoveryTime_=0,StaggerTime_=0,StaggerCooldown_=0,PoiseDamage_=0,PoiseReset_=0;
	TArray<FTransform> RestPose_;
	TMap<FName, int32> AnimatedBones_;

	FVector Home_ = FVector::ZeroVector;
	FVector MoveTarget_ = FVector::ZeroVector;
	float ThinkTime_ = 0.0f;
	float AttackCooldown_ = 0.0f;
	float DeathTime_ = 0.0f;
	float FarAcc_ = 0.0f;
	FCultControlProjection Control_;

	class ACultUlhuPlayerPawn* Player_ = nullptr;
};
