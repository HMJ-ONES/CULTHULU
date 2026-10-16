// CULT-ULHU: cult roster, per world.
//
// Owns the CultManager (cultists, loyalty, insurrection). World-scoped:
// a new match/level gets a fresh cult. Pulls the shared bus/clock/rng
// from UCultBeliefSubsystem in Initialize().
//
// Core mapping: src/cult/CultManager.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "cult/CultManager.h"

#include "CultManagerSubsystem.generated.h"

UCLASS()
class CULTULHUCORE_API UCultManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	cultulhu::CultManager* Cult() { return Cult_.get(); }

	UFUNCTION(BlueprintPure, Category = "CultUlhu|Cult")
	int32 GetCultistCount() const;

	UFUNCTION(BlueprintPure, Category = "CultUlhu|Cult")
	float GetInsurrectionRisk() const;

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Sim")
	void TickSim(float DeltaSeconds);

private:
	std::unique_ptr<cultulhu::CultManager> Cult_;
};
