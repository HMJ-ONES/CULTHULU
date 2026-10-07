// CULT-ULHU: power + exertion.
//
// Owns PowerSystem and ExertionSystem. The ExertionSystem needs a
// CultManager, which is world-scoped (UCultManagerSubsystem), so it is
// bound lazily via BindCult() — the GameMode calls this once the world
// subsystem exists, before the first tick.
//
// Core mapping: src/power/PowerSystem.h, src/exertion/ExertionSystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "power/PowerSystem.h"
#include "exertion/ExertionSystem.h"

#include "CultPowerSubsystem.generated.h"

namespace cultulhu { class CultManager; }

UCLASS()
class CULTULHUCORE_API UCultPowerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Called by the GameMode after the world (and its UCultManagerSubsystem)
	// exists. Safe to call repeatedly; binds once.
	void BindCult(cultulhu::CultManager& Cult);

	cultulhu::PowerSystem& Power() { return Power_; }
	cultulhu::ExertionSystem* Exertion() { return Exertion_.get(); }

	UFUNCTION(BlueprintPure, Category = "CultUlhu|Power")
	float GetPower() const { return Power_.value(); }

	// BeliefIndex: 0..11 in Belief enum order (see src/beliefs/Belief.h).
	UFUNCTION(BlueprintPure, Category = "CultUlhu|Power")
	float GetExertion(int32 BeliefIndex) const;

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Sim")
	void TickSim(float DeltaSeconds);

private:
	cultulhu::PowerSystem Power_;
	std::unique_ptr<cultulhu::ExertionSystem> Exertion_;
};
