// CULT-ULHU: game-instance-level simulation state.
//
// Owns the engine-agnostic core objects that are global to the game
// session (not per-world): EventBus, GameClock, RNG, BeliefSystem.
// Sibling subsystems (power, cult) pull these from here in Initialize().
//
// Core mapping: src/beliefs/BeliefSystem.h, src/core/EventBus.h,
// src/core/GameClock.h, src/core/RNG.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

// Core includes (include path added by CultUlhuCore.Build.cs).
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "beliefs/BeliefSystem.h"

#include "CultBeliefSubsystem.generated.h"

UCLASS()
class CULTULHUCORE_API UCultBeliefSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- Core access (game thread only) ----
	cultulhu::EventBus& Bus() { return *Bus_; }
	cultulhu::GameClock& Clock() { return *Clock_; }
	cultulhu::RNG& Rng() { return *Rng_; }
	cultulhu::BeliefSystem& Beliefs() { return *Beliefs_; }

	// ---- Blueprint-friendly reads ----
	// (Per-belief exertion gauges live on UCultPowerSubsystem, which owns
	// the ExertionSystem.)
	UFUNCTION(BlueprintPure, Category = "CultUlhu|Beliefs")
	TArray<FString> GetActiveBeliefNames() const;

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Sim")
	void TickSim(float DeltaSeconds);

private:
	// VERIFY IN EDITOR: std::unique_ptr members inside a UCLASS compile
	// cleanly with the engine's STL settings (they should; <memory> is fine).
	std::unique_ptr<cultulhu::EventBus> Bus_;
	std::unique_ptr<cultulhu::GameClock> Clock_;
	std::unique_ptr<cultulhu::RNG> Rng_;
	std::unique_ptr<cultulhu::BeliefSystem> Beliefs_;
};
