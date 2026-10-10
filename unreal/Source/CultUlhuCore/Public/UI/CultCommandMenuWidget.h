// CULT-ULHU: radial command menu widget (Alt + left-click).
//
// C++ side of the UMG widget. The core CommandMenu (src/ui/CommandMenu.h)
// is UI-agnostic: generateMenu() returns data (MenuButton list), dispatch()
// executes the choice. This widget builds the buttons at runtime and
// positions them radially around the click point.
//
// The visual layout (radial arrangement, styling) is authored in the
// Widget Blueprint; see Docs/UmgWidgetSpecs.md for the precise spec.
// VERIFY IN EDITOR: the Widget Blueprint parents this class and implements
// the layout spec; UCanvasPanel* MenuRoot must exist with that exact name.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ui/CommandMenu.h"

#include "CultCommandMenuWidget.generated.h"

class UCanvasPanel;
class UButton;
class UTextBlock;

UCLASS(Blueprintable)
class CULTULHUCORE_API UCultCommandMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Opens the menu for a core menu context at a screen position.
	// The game layer fills MenuContext from the Alt+click pick result.
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Menu")
	void OpenMenu(const FVector2D& ScreenPos);

	// Closes without choosing (also on right-click / Esc).
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Menu")
	void CloseMenu();

protected:
	virtual void NativeConstruct() override;

	// Bound in the Widget Blueprint (exact names required).
	// VERIFY IN EDITOR: BindWidget names match the Blueprint.
	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* MenuRoot = nullptr;

private:
	// Builds one UButton per core MenuButton and lays them out radially.
	// VERIFY IN EDITOR: button widget class (WBP_CommandMenuButton) exists
	// and exposes SetLabel(); adjust the class reference here.
	void RebuildButtons(const std::vector<cultulhu::MenuButton>& Buttons);

	void OnMenuButtonClicked(int32 ButtonIndex);

	std::vector<cultulhu::MenuButton> CurrentButtons_;
	cultulhu::CommandMenu MenuModel_;
	bool bOpen_ = false;
};
