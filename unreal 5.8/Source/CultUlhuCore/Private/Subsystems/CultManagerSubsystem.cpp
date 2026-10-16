// CULT-ULHU cult manager subsystem implementation.
#include "Subsystems/CultManagerSubsystem.h"
#include "Subsystems/CultBeliefSubsystem.h"

void UCultManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UCultBeliefSubsystem* Core = GetGameInstance()->GetSubsystem<UCultBeliefSubsystem>();
	// VERIFY IN EDITOR: world subsystems initialize after game-instance
	// subsystems, so Core must be non-null here.
	check(Core);
	Cult_ = std::make_unique<cultulhu::CultManager>(
		Core->Bus(), Core->Clock(), Core->Rng());
}

void UCultManagerSubsystem::Deinitialize()
{
	Cult_.reset();
	Super::Deinitialize();
}

int32 UCultManagerSubsystem::GetCultistCount() const
{
	return Cult_ ? static_cast<int32>(Cult_->size()) : 0;
}

float UCultManagerSubsystem::GetInsurrectionRisk() const
{
	return Cult_ ? Cult_->insurrectionRisk() : 0.0f;
}

void UCultManagerSubsystem::TickSim(float DeltaSeconds)
{
	if (Cult_) Cult_->update(DeltaSeconds);
}
