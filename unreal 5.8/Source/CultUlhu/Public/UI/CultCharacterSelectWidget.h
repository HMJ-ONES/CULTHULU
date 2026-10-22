// CULT-ULHU character select: released entity roster and original kit details.
// Pure C++ UMG. SetMode() before RebuildGrid(); SelectCharacter() is
// called by the cards; OnPlay starts the game via the game instance.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CultUlhuGameInstance.h"

#include "CultCharacterSelectWidget.generated.h"

class UCanvasPanel;
class UVerticalBox;
class UUniformGridPanel;
class UScrollBox;
class UButton;
class UTextBlock;
class UCultCharacterCardWidget;

UCLASS()
class CULTULHU_API UCultCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// See CultHomeScreenWidget::RebuildWidget — the UWidget tree must be
	// built at Slate-build time, not in NativeConstruct.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void SetMode(ECultPlayMode Mode) { Mode_ = Mode; }

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Menu")
	void RebuildGrid();

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Menu")
	void SelectCharacter(int32 Index);

protected:
	UFUNCTION() void OnBackClicked();
	UFUNCTION() void OnPlayClicked();

private:
	friend struct FCultCharacterSelectionTestAccess;
	void BuildLayout();
	void RefreshSelection();

	ECultPlayMode Mode_ = ECultPlayMode::FreeRoam;
	int32 SelectedIndex_ = 2; // default: Cthulhu

	UPROPERTY() UCanvasPanel* Root_ = nullptr;
	UPROPERTY() UUniformGridPanel* Grid_ = nullptr;
	UPROPERTY() UTextBlock* TitleText_ = nullptr;
	UPROPERTY() UTextBlock* DetailText_ = nullptr;
	UPROPERTY() UTextBlock* DetailSubtitle_ = nullptr;
	UPROPERTY() UTextBlock* DetailFlavor_ = nullptr;
	UPROPERTY() UTextBlock* DetailStats_ = nullptr;
	UPROPERTY() UTextBlock* ModeHint_ = nullptr;
	UPROPERTY() TArray<UTextBlock*> AbilityNames_;
	UPROPERTY() TArray<UTextBlock*> AbilityMeta_;
	UPROPERTY() UButton* BackButton_ = nullptr;
	UPROPERTY() UButton* PlayButton_ = nullptr;
	UPROPERTY() TArray<UCultCharacterCardWidget*> Cards_;
	bool bBuilt_ = false;
};
