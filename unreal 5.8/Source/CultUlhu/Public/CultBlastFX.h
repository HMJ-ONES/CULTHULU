// CULT-ULHU blast FX: short-lived, self-animating impact effects.
//
// Hit flashes and shock rings use one open procedural signal mesh. Collapse
// dust retains its original puffs. All effects are cosmetic and noncolliding.
//
// Spawn via ACultBlastFX::SpawnBlast (deferred spawn sets the kind before
// BeginPlay). The actor destroys itself when the last puff dies. Missing
// materials/meshes degrade to untextured shapes — never a crash.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CultBlastFX.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ECultBlastKind : uint8
{
	ShockRing,
	DustBurst,
	HitFlash // single bright pop at a damage hit (buildings, beasts)
};

UCLASS()
class CULTULHU_API ACultBlastFX : public AActor
{
	GENERATED_BODY()

public:
	ACultBlastFX();

	static ACultBlastFX* SpawnBlast(UWorld* World, ECultBlastKind Kind,
		FVector Location, float RadiusM, FVector ImpactDirection=FVector::UpVector);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	friend struct FCultImpactPresentationTestAccess;
	void BuildPuffs();
	void BuildSignal();
	void UpdateSignal();
	UPROPERTY() class UProceduralMeshComponent* Signal_=nullptr;
	UPROPERTY() class UMaterialInstanceDynamic* SignalMaterial_=nullptr;
	FVector ImpactDirection_=FVector::UpVector;
	float SignalAge_=0,SignalDuration_=.22f,LegacyRadiusM_=2;
	bool Refined_=false;

	struct FPuff
	{
		UStaticMeshComponent* Comp = nullptr;
		FVector Vel = FVector::ZeroVector;
		float Life = 0.0f;
		float MaxLife = 1.0f;
		float BaseScale = 1.0f;
	};
	TArray<FPuff> Puffs_;

	// Central flash for ShockRing.
	UStaticMeshComponent* Flash_ = nullptr;
	float FlashLife_ = 0.0f;
	float FlashMaxLife_ = 0.45f;

	ECultBlastKind Kind_ = ECultBlastKind::DustBurst;
	float RadiusM_ = 10.0f;
};
