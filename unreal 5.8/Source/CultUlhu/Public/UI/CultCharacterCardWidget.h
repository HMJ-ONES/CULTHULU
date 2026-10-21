// CULT-ULHU character card: one clickable card in the select grid.
// Pure C++ UMG; spawned by UCultCharacterSelectWidget::RebuildGrid.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CultCharacterCardWidget.generated.h"

class UButton;
class UTextBlock;
class UCultCharacterSelectWidget;

UCLASS()
class CULTULHU_API UCultCharacterCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// Builds the card visuals; must be called before AddToViewport.
	void Setup(int32 InIndex, const FString& Name, const FString& Flavor,
	           UCultCharacterSelectWidget* Parent);

	void SetSelected(bool bSelected);

protected:
	UFUNCTION() void OnCardClicked();

private:
	int32 Index_ = -1;
	UPROPERTY() UButton* CardButton_ = nullptr;
	UPROPERTY() UTextBlock* NameText_ = nullptr;
	UPROPERTY() UCultCharacterSelectWidget* Parent_ = nullptr;
};
