// CULT-ULHU game instance: owns the menu -> character select -> play flow.
//
// The home screen and character select are pure C++ UUserWidgets (no
// Widget Blueprints needed). The instance remembers the chosen mode and
// character; ACultUlhuPlayGameMode reads them when the world loads.
//
// VERIFY IN EDITOR: set as the Game Instance class in
// Project Settings -> Maps & Modes (or Config/DefaultEngine.ini).
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"

#include "CultUlhuGameInstance.generated.h"

// Playable modes. Names match the driver (`match domination`, `match dotgoo`);
// "Onslaught" stays a belief, not a mode.
UENUM(BlueprintType)
enum class ECultPlayMode : uint8
{
	FreeRoam   UMETA(DisplayName = "Free Roam"),
	Domination UMETA(DisplayName = "Domination (5v5)"),
	Dotgoo     UMETA(DisplayName = "Defense of the Great Old Ones (5v5)")
};

// One playable entity for the character select grid.
USTRUCT(BlueprintType)
struct FCultRosterEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	UPROPERTY(BlueprintReadOnly) FString Flavor;
	// Canonical kaiju height in meters (1 unit = 1 m modeling rule).
	UPROPERTY(BlueprintReadOnly) float HeightM = 20.0f;
};

UCLASS(Blueprintable)
class CULTULHU_API UCultUlhuGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
 virtual void Init() override;
 virtual void Shutdown() override;
 FString FreeRoamSaveSlot=TEXT("FreeRoam");
 int32 FreeRoamSeed=0;
 bool FreeRoamMenuOpen=false;
 bool FreeRoamReady=false;
 FString SaveStatus;
 void ToggleFreeRoamMenu();
 void ResumeFreeRoam();
 bool SaveFreeRoam();
 bool ContinueFreeRoam();
 void FinishFreeRoamLoad();
 void ExitFreeRoam();
 bool HostFreeRoam();
 bool JoinFreeRoam(const FString& Address);
 bool HasFreeRoamSave() const;
 void BeginFreeRoamSession();
	bool bPlayChosen = false;
	// Current release roster; deferred character source files remain in assets/characters.
	static const TArray<FCultRosterEntry>& Roster();

	// ---- menu flow (called from the widgets / pawn) ----
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Menu")
	void ShowHomeScreen();

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Menu")
	void OpenCharacterSelect(ECultPlayMode Mode);

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Menu")
	void StartPlay();
 void OpenBeliefSelect();
 bool SetInitialBeliefs(const TArray<int32>& Choices);
 const TArray<int32>& InitialBeliefs() const { return InitialBeliefs_; }

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Menu")
	void ReturnToMenu();

	// ---- selections ----
	UPROPERTY(BlueprintReadOnly, Category = "CultUlhu|Menu")
	ECultPlayMode SelectedMode = ECultPlayMode::FreeRoam;

	UPROPERTY(BlueprintReadOnly, Category = "CultUlhu|Menu")
	FString SelectedCharacterId = TEXT("cthulhu_avatar");

	static FString ModeDisplayName(ECultPlayMode Mode);
	static FString ModeStartLabel(ECultPlayMode Mode);

private:
 void NetworkFailure(UWorld* World,UNetDriver* Driver,ENetworkFailure::Type Failure,const FString& Reason);
 TArray<uint8> PendingFreeRoam_;
 FTimerHandle AutosaveTimer_;
	TArray<int32> InitialBeliefs_;
	void ShowWidget(UUserWidget* Widget, bool bUIOnly);
	void ClearMenuWidgets();

	UPROPERTY()
	TArray<UUserWidget*> MenuWidgets_;
};
