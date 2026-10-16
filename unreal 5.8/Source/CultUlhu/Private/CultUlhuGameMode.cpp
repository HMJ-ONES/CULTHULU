// CULT-ULHU game mode implementation.
#include "CultUlhuGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystems/CultBeliefSubsystem.h"
#include "Subsystems/CultPowerSubsystem.h"
#include "Subsystems/CultManagerSubsystem.h"

ACultUlhuGameMode::ACultUlhuGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	// VERIFY IN EDITOR: bUseSeamlessTravel as desired for map changes.
}

void ACultUlhuGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void ACultUlhuGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	UCultBeliefSubsystem* Beliefs = GI->GetSubsystem<UCultBeliefSubsystem>();
	UCultPowerSubsystem* Power = GI->GetSubsystem<UCultPowerSubsystem>();
	// World subsystem: valid once the world exists (it does, in Tick).
	UCultManagerSubsystem* Cult = GetWorld()->GetSubsystem<UCultManagerSubsystem>();
	if (!Beliefs || !Power || !Cult || !Cult->Cult()) return;

	if (!bCultBound_)
	{
		Power->BindCult(*Cult->Cult());
		bCultBound_ = true;
	}

	// Dependency order: clock/adoption -> cult roster -> exertion/power.
	Beliefs->TickSim(DeltaSeconds);
	Cult->TickSim(DeltaSeconds);
	Power->TickSim(DeltaSeconds);
}

void ACultUlhuGameMode::HostListenServer(const FString& MapName)
{
	// VERIFY IN EDITOR: test over Radmin (26.x.x.x). The listen server
	// opens <GamePort> (default 7777; set via -port= or URL option).
	const FString Url = MapName + TEXT("?listen");
	UGameplayStatics::OpenLevel(this, FName(*Url));
}
