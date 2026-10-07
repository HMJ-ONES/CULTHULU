// CULT-ULHU belief subsystem implementation.
#include "Subsystems/CultBeliefSubsystem.h"
#include "beliefs/Belief.h"
#include "beliefs/BeliefSystem.h"

void UCultBeliefSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Bus_ = std::make_unique<cultulhu::EventBus>();
	Clock_ = std::make_unique<cultulhu::GameClock>();
	// VERIFY IN EDITOR: pick seeding policy — fixed seed for replays,
	// FMath::Rand() / random_device for live play. Fixed here to match
	// the headless driver.
	Rng_ = std::make_unique<cultulhu::RNG>(1234);
	Beliefs_ = std::make_unique<cultulhu::BeliefSystem>(*Bus_, *Clock_);
}

void UCultBeliefSubsystem::Deinitialize()
{
	Beliefs_.reset();
	Rng_.reset();
	Clock_.reset();
	Bus_.reset();
	Super::Deinitialize();
}

TArray<FString> UCultBeliefSubsystem::GetActiveBeliefNames() const
{
	TArray<FString> Out;
	if (!Beliefs_) return Out;
	for (cultulhu::Belief b : Beliefs_->active())
		Out.Add(FString(cultulhu::beliefName(b)));
	return Out;
}

void UCultBeliefSubsystem::TickSim(float DeltaSeconds)
{
	if (!Clock_ || !Beliefs_) return;
	Clock_->advance(DeltaSeconds);
	Beliefs_->update(DeltaSeconds);
}
