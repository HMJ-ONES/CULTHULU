// CULT-ULHU power subsystem implementation.
#include "Subsystems/CultPowerSubsystem.h"
#include "Subsystems/CultBeliefSubsystem.h"
#include "beliefs/Belief.h"

void UCultPowerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// PowerSystem default-constructs (START_POWER = 100).
	// ExertionSystem binds later in BindCult().
}

void UCultPowerSubsystem::Deinitialize()
{
	Exertion_.reset();
	Super::Deinitialize();
}

void UCultPowerSubsystem::BindCult(cultulhu::CultManager& Cult)
{
	if (Exertion_) return;
	UCultBeliefSubsystem* Core = GetGameInstance()->GetSubsystem<UCultBeliefSubsystem>();
	// VERIFY IN EDITOR: GetGameInstance() is valid here (called from
	// GameMode after world init). If null, defer binding to first TickSim.
	check(Core);
	Exertion_ = std::make_unique<cultulhu::ExertionSystem>(
		Core->Bus(), Core->Beliefs(), Power_, Cult, Core->Rng());
}

float UCultPowerSubsystem::GetExertion(int32 BeliefIndex) const
{
	if (!Exertion_) return 0.0f;
	if (BeliefIndex < 0 || BeliefIndex >= static_cast<int32>(cultulhu::Belief::Count))
		return 0.0f;
	return Exertion_->exertion(static_cast<cultulhu::Belief>(BeliefIndex));
}

void UCultPowerSubsystem::TickSim(float DeltaSeconds)
{
	if (Exertion_) Exertion_->update(DeltaSeconds);
}
