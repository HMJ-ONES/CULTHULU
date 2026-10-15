// CULT-ULHU destructible building: a city structure with hit points.
//
// Spawned by unreal 5.8/Tools/BuildDarkWilds.py for the razed-city structures
// (columns, walls, towers, crypts, arches, obelisks). The pawn's smash
// (ACultUlhuPlayerPawn::Smash) calls ApplyDamage; at zero HP the building
// collapses: the intact mesh hides, a rubble mound stays behind, debris
// chunks fly on manual ballistics, and a dust burst plays.
//
// No Chaos fracture, no replication — free-roam test build. HP scales with
// the mesh bounds so towers take several smashes and columns fall in one.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CultDestructible.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class CULTULHU_API ACultDestructible : public AActor
{
	GENERATED_BODY()

public:
	ACultDestructible();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Called by the build script right after spawning (mesh asset already
	// imported by BuildDarkWilds.py).
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Destruction")
	void SetMesh(UStaticMesh* InMesh);

	// Deals damage; HitLocation is informational (dust/debris origin bias).
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Destruction")
	void ApplyDamage(float Amount, FVector HitLocation);

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Destruction")
	bool IsDestroyed() const { return bDestroyed_; }

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Destruction")
	float GetHealth() const { return Health_; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Destruction")
	UStaticMeshComponent* MeshComp_ = nullptr;

	// Tuning knob per building (1.0 = default).
	UPROPERTY(EditAnywhere, Category = "CultUlhu|Destruction", meta = (ClampMin = "0.1"))
	float HealthMultiplier_ = 1.0f;

private:
	void Collapse(FVector HitLocation);
	void SpawnDebris(FVector Center, float RadiusCm);
	static UStaticMesh* SharedMesh(const TCHAR* Path);

	float Health_ = 100.0f;
	float MaxHealth_ = 100.0f;
	bool bDestroyed_ = false;

	struct FChunk
	{
		UStaticMeshComponent* Comp = nullptr;
		FVector Vel = FVector::ZeroVector;
		FVector Spin = FVector::ZeroVector; // euler deg/sec
		float Life = 0.0f;
		float MaxLife = 1.0f;
		float BaseScale = 1.0f;
	};
	TArray<FChunk> Chunks_;
};
