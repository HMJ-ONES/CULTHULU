// CULT-ULHU command menu widget implementation.
#include "UI/CultCommandMenuWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UCultCommandMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UCultCommandMenuWidget::OpenMenu(const FVector2D& ScreenPos)
{
	// VERIFY IN EDITOR: the game layer must fill a cultulhu::MenuContext
	// (clicked entity / location / altar) before calling OpenMenu; wire the
	// Alt+left-click pick in the PlayerController (see Docs/UmgWidgetSpecs).
	cultulhu::MenuContext Ctx;
	// auto Buttons = MenuModel_.generateMenu(Ctx, /*summary*/ ...);
	// RebuildButtons(Buttons);

	SetVisibility(ESlateVisibility::Visible);
	bOpen_ = true;
	(void)ScreenPos;
}

void UCultCommandMenuWidget::CloseMenu()
{
	SetVisibility(ESlateVisibility::Collapsed);
	bOpen_ = false;
	CurrentButtons_.clear();
}

void UCultCommandMenuWidget::RebuildButtons(const std::vector<cultulhu::MenuButton>& Buttons)
{
	CurrentButtons_ = Buttons;
	// VERIFY IN EDITOR: implement radial layout — N buttons evenly spaced
	// on a circle of radius ~120px around the click point; see the layout
	// spec in Docs/UmgWidgetSpecs.md. Each button calls
	// OnMenuButtonClicked(Index); dispatch via
	// MenuModel_.dispatch(CurrentButtons_[Index], Ctx, ...).
}

void UCultCommandMenuWidget::OnMenuButtonClicked(int32 ButtonIndex)
{
	if (ButtonIndex < 0 || ButtonIndex >= static_cast<int32>(CurrentButtons_.size()))
		return;
	// VERIFY IN EDITOR: call MenuModel_.dispatch() with the stored context,
	// then CloseMenu().
	CloseMenu();
}
