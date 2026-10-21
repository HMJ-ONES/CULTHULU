#include "UI/CultCharacterCardWidget.h"
#include "Blueprint/WidgetTree.h"
// CULT-ULHU character card implementation.
#include "UI/CultCharacterCardWidget.h"

#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "UI/CultCharacterSelectWidget.h"

#include "UI/CultUIStyle.h"

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
	CultUI::Button(CardButton_);
	CardButton_->OnClicked.AddDynamic(this, &UCultCharacterCardWidget::OnCardClicked);

	UVerticalBox* V = NewObject<UVerticalBox>(CardButton_);
	NameText_ = NewObject<UTextBlock>(V);
	NameText_->SetText(FText::FromString(Name.ToUpper()));
	FSlateFontInfo F = CultUI::Font(20,true);
	NameText_->SetFont(F);
	NameText_->SetColorAndOpacity(FSlateColor(CultUI::Bone));
	NameText_->SetJustification(ETextJustify::Left);
	NameText_->SetAutoWrapText(true);

	UTextBlock* Fl = NewObject<UTextBlock>(V);
	Fl->SetText(FText::FromString(Flavor));
	FSlateFontInfo F2 = CultUI::Font(12);
	Fl->SetFont(F2);
	Fl->SetColorAndOpacity(FSlateColor(CultUI::Muted));
	Fl->SetJustification(ETextJustify::Left);
	Fl->SetAutoWrapText(true);

	if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(NameText_)) VS->SetPadding(FMargin(10, 10, 10, 2));
	if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(Fl)) VS->SetPadding(FMargin(10, 0, 10, 10));
	if(auto* ContentSlot=Cast<UButtonSlot>(CardButton_->AddChild(V))) ContentSlot->SetHorizontalAlignment(HAlign_Fill);

	if (!WidgetTree)
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	WidgetTree->RootWidget = CardButton_;
}

void UCultCharacterCardWidget::SetSelected(bool bSelected)
{
	if (!CardButton_) return;
	CultUI::Button(CardButton_,false,bSelected);
 NameText_->SetColorAndOpacity(FSlateColor(bSelected?CultUI::Teal:CultUI::Bone));
}

void UCultCharacterCardWidget::OnCardClicked()
{
	if (Parent_) Parent_->SelectCharacter(Index_);
}

