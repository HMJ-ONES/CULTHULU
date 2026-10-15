// CULT-ULHU feral beast: simple live AI for the Dark Wilds.
//
// Spawned by unreal/Tools/BuildDarkWilds.py from the `spawn` entries in
// dark_wilds.map (species, position, count, zone), replacing the old static
// display meshes. Wander / chase / swipe / die — no navmesh, no physics:
// beasts steer on the horizontal plane at their spawn height, which keeps
// them cheap enough for low-spec hosts (30 beasts x trivial tick).
//
// Damageable via ApplyDamage (the pawn's smash calls it). Swipe is visual
// only for now — player HP is a P1 release-checklist item; the lunge lands
// a small camera kick so it still reads as an attack.
#pragma once

#include "CoreMinimal.h"
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

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Beast")
	bool IsDead() const { return State_ == EState::Dead || State_ == EState::Dying; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Beast")
	UStaticMeshComponent* MeshComp_ = nullptr;

	// Glowing predator eyes (HDR material, positioned from mesh bounds).
	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Beast")
	UStaticMeshComponent* EyeL_ = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Beast")
	UStaticMeshComponent* EyeR_ = nullptr;

private:
	enum class EState : uint8 { Wander, Chase, Dying, Dead };
	EState State_ = EState::Wander;

	void ApplySpeciesStats();
	void PickWanderTarget();
	void AttachEyes();
	void MoveToward(const FVector& Dest, float SpeedMs, float DeltaSeconds);
	void FaceToward(const FVector& Dest, float DeltaSeconds, float Rate = 6.0f);
	void Die();

	FString SpeciesId_;
	float Health_ = 60.0f;
	float MaxHealth_ = 60.0f;
	float Speed_ = 5.0f;   // m/s
	float BodyScale_ = 1.0f;

	FVector Home_ = FVector::ZeroVector;
	FVector MoveTarget_ = FVector::ZeroVector;
	float ThinkTime_ = 0.0f;
	float AttackCooldown_ = 0.0f;
	float DeathTime_ = 0.0f;
	float FarAcc_ = 0.0f;

	class ACultUlhuPlayerPawn* Player_ = nullptr;
};
