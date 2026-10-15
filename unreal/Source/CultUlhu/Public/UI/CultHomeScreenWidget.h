// CULT-ULHU home screen: mode selection. Pure C++ UMG (no Blueprint).
//
// Layout is built in NativeConstruct: full-screen dark backdrop, title,
// three mode buttons. Clicks route through UCultUlhuGameInstance.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CultUlhuGameInstance.h"

#include "CultHomeScreenWidget.generated.h"

class UCanvasPanel;
class UVerticalBox;
class UButton;
class UTextBlock;

UCLASS()
class CULTULHU_API UCultHomeScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Builds the UWidget tree at Slate-build time. NOTE: the tree must be
	// built here, NOT in NativeConstruct — RebuildWidget() is what produces
	// the Slate widget and it runs before NativeConstruct, so a
	// WidgetTree->RootWidget assigned there would never be taken, leaving
	// an invisible menu.
	virtual TSharedRef<SWidget> RebuildWidget() override;

protected:
	UFUNCTION() void OnFreeRoamClicked();
	UFUNCTION() void OnDominationClicked();
	UFUNCTION() void OnDotgooClicked();

private:
	void BuildLayout();
	UButton* MakeModeButton(const FString& Title, const FString& Sub);

	UPROPERTY() UCanvasPanel* Root_ = nullptr;
	UPROPERTY() UVerticalBox* ButtonBox_ = nullptr;
	bool bBuilt_ = false;
};
