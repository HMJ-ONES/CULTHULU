// CULT-ULHU blast FX: short-lived, self-animating impact effects.
//
// Two kinds, both built from Engine basic shapes + the FX materials made
// by unreal 5.8/Tools/BuildDarkWilds.py (M_FX_Dust, M_FX_Flash):
//   ShockRing — the pawn's smash: 12 dust puffs racing outward in a ring
//               plus a central flash that pops. Fades by shrinking.
//   DustBurst — building collapse: dust puffs blooming up and out.
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
		FVector Location, float RadiusM);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void BuildPuffs();

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
