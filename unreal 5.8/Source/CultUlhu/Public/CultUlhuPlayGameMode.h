// CULT-ULHU play game mode: menu -> world flow + core sim tick.
//
// Extends ACultUlhuGameMode (keeps the core tick order) with:
//  - DefaultPawnClass = ACultUlhuPlayerPawn (movement + FP/TP camera)
//  - ChoosePlayerStart prefers the mode's WILD_PlayerStart_* label
//    (placed by BuildDarkWilds.py); falls back to default selection.
//  - On the menu map, shows the home screen instead of spawning.
//
// VERIFY IN EDITOR: set as the default GameMode (Config/DefaultEngine.ini
// does this) and confirm the pawn spawns at the plateau start on Play.
#pragma once

#include "CoreMinimal.h"
#include "CultUlhuGameMode.h"

#include "CultUlhuPlayGameMode.generated.h"

UCLASS(Blueprintable)
class CULTULHU_API ACultUlhuPlayGameMode : public ACultUlhuGameMode
{
	GENERATED_BODY()

public:
	ACultUlhuPlayGameMode();
	virtual void InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage) override;

	virtual void BeginPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	// Menu map: UI only — suppress the default pawn so nothing spawns,
	// falls, or steals the camera behind the menu vista.
	virtual APawn* SpawnDefaultPawnFor_Implementation(
		AController* NewPlayer, AActor* StartSpot) override;
};
