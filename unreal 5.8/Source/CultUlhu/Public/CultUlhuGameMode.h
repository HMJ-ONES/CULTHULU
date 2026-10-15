// CULT-ULHU game mode: sim tick order + listen-server session.
//
// Ticks the core simulation every frame in dependency order:
//   Beliefs (clock/adoption) -> CultManager -> Power/Exertion
// and binds the ExertionSystem to the world CultManager once.
//
// Multiplayer: this game uses a LISTEN SERVER (one player's machine hosts,
// matching the Radmin virtual-LAN design). See Docs/NetcodeDecision.md.
// VERIFY IN EDITOR: set as GameMode in the project / map settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "CultUlhuGameMode.generated.h"

UCLASS(Blueprintable)
class CULTULHU_API ACultUlhuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACultUlhuGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Host a listen-server session: call from the main-menu "Host" button.
	// Clients join with: open <Radmin-IP> (e.g. "open 26.12.34.56").
	// See Docs/NetcodeDecision.md for the Radmin setup.
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Net")
	void HostListenServer(const FString& MapName);

protected:
	// VERIFY IN EDITOR: default pawn = your ACultUlhuCharacter Blueprint
	// (set DefaultPawnClass here or in the Blueprint defaults).
	// VERIFY IN EDITOR: PlayerControllerClass = ACultUlhuPlayerController.

private:
	bool bCultBound_ = false;
};
