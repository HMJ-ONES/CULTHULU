// CULT-ULHU character card implementation.
#include "UI/CultCharacterCardWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "UI/CultCharacterSelectWidget.h"

namespace
{
	const FLinearColor kCard(0.07f, 0.05f, 0.11f, 1.0f);
	const FLinearColor kCardSel(0.30f, 0.18f, 0.48f, 1.0f);
	const FLinearColor kCardHover(0.16f, 0.10f, 0.26f, 1.0f);
	const FLinearColor kBone(0.82f, 0.80f, 0.74f, 1.0f);
	const FLinearColor kDim(0.55f, 0.52f, 0.60f, 1.0f);
}

void UCultCharacterCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCultCharacterCardWidget::Setup(int32 InIndex, const FString& Name,
                                     const FString& Flavor,
                                     UCultCharacterSelectWidget* Parent)
{
	Index_ = InIndex;
	Parent_ = Parent;

	CardButton_ = NewObject<UButton>(this);
	FButtonStyle S = CardButton_->GetStyle();
	S.Normal.TintColor = FSlateColor(kCard);
	S.Hovered.TintColor = FSlateColor(kCardHover);
	S.Pressed.TintColor = FSlateColor(kCardSel);
	CardButton_->SetStyle(S);
	CardButton_->OnClicked.AddDynamic(this, &UCultCharacterCardWidget::OnCardClicked);

	UVerticalBox* V = NewObject<UVerticalBox>(CardButton_);
	NameText_ = NewObject<UTextBlock>(V);
	NameText_->SetText(FText::FromString(Name));
	FSlateFontInfo F = NameText_->GetFont();
	F.Size = 18;
	NameText_->SetFont(F);
	NameText_->SetColorAndOpacity(FSlateColor(kBone));
	NameText_->SetJustification(ETextJustify::Center);
	NameText_->SetAutoWrapText(true);

	UTextBlock* Fl = NewObject<UTextBlock>(V);
	Fl->SetText(FText::FromString(Flavor));
	FSlateFontInfo F2 = Fl->GetFont();
	F2.Size = 12;
	Fl->SetFont(F2);
	Fl->SetColorAndOpacity(FSlateColor(kDim));
	Fl->SetJustification(ETextJustify::Center);
	Fl->SetAutoWrapText(true);

	if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(NameText_)) VS->SetPadding(FMargin(10, 10, 10, 2));
	if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(Fl)) VS->SetPadding(FMargin(10, 0, 10, 10));
	CardButton_->AddChild(V);

	WidgetTree->RootWidget = CardButton_;
}

void UCultCharacterCardWidget::SetSelected(bool bSelected)
{
	if (!CardButton_) return;
	FButtonStyle S = CardButton_->GetStyle();
	S.Normal.TintColor = FSlateColor(bSelected ? kCardSel : kCard);
	CardButton_->SetStyle(S);
}

void UCultCharacterCardWidget::OnCardClicked()
{
	if (Parent_) Parent_->SelectCharacter(Index_);
}
