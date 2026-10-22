#include "UI/CultCharacterCardWidget.h"
#include "Blueprint/WidgetTree.h"
// CULT-ULHU character card implementation.
#include "UI/CultCharacterCardWidget.h"

#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"

#include "UI/CultCharacterSelectWidget.h"

#include "UI/CultUIStyle.h"
namespace {
void StyleCard(UButton* Button,bool Selected=false) {
 CultUI::Button(Button,false,Selected); auto Style=Button->GetStyle();
 Style.NormalPadding=FMargin(12,10); Style.PressedPadding=FMargin(12,11,12,9);
 Button->SetStyle(Style); Button->SetColorAndOpacity(FLinearColor::White);
}
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
	StyleCard(CardButton_);
	CardButton_->OnClicked.AddDynamic(this, &UCultCharacterCardWidget::OnCardClicked);
	CardButton_->SetToolTipText(FText::FromString(Name+TEXT("\n")+Flavor));
	FString Short=Name,Subtitle; int32 Comma;
	if(Name.FindChar(',',Comma)) { Short=Name.Left(Comma); Subtitle=Name.Mid(Comma+1).TrimStartAndEnd(); }

	UVerticalBox* V = NewObject<UVerticalBox>(CardButton_);
	NameText_ = NewObject<UTextBlock>(V);
	NameText_->SetText(FText::FromString(Short.ToUpper()));
	FSlateFontInfo F = CultUI::Font(18,true);
	NameText_->SetFont(F);
	NameText_->SetColorAndOpacity(FSlateColor(CultUI::Bone));
	NameText_->SetJustification(ETextJustify::Left);
	NameText_->SetAutoWrapText(false);

	UTextBlock* Fl = NewObject<UTextBlock>(V);
	Fl->SetText(FText::FromString(Subtitle));
	FSlateFontInfo F2 = CultUI::Font(11);
	Fl->SetFont(F2);
	Fl->SetColorAndOpacity(FSlateColor(CultUI::Muted));
	Fl->SetJustification(ETextJustify::Left);
	Fl->SetAutoWrapText(true);

	auto* NameSize=NewObject<USizeBox>(V); NameSize->SetHeightOverride(28);
	auto* Fit=NewObject<UScaleBox>(V); Fit->SetStretch(EStretch::ScaleToFit); Fit->SetStretchDirection(EStretchDirection::DownOnly); NameSize->AddChild(Fit);
	if(auto* S=Cast<UScaleBoxSlot>(Fit->AddChild(NameText_))) { S->SetHorizontalAlignment(HAlign_Left); S->SetVerticalAlignment(VAlign_Center); }
	if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(NameSize)) VS->SetPadding(FMargin(0,0,0,6));
	V->AddChildToVerticalBox(Fl);
	if(auto* ContentSlot=Cast<UButtonSlot>(CardButton_->AddChild(V))) ContentSlot->SetHorizontalAlignment(HAlign_Fill);

	if (!WidgetTree)
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	WidgetTree->RootWidget = CardButton_;
}

void UCultCharacterCardWidget::SetSelected(bool bSelected)
{
	if (!CardButton_) return;
	StyleCard(CardButton_,bSelected);
 NameText_->SetColorAndOpacity(FSlateColor(bSelected?CultUI::Teal:CultUI::Bone));
}

void UCultCharacterCardWidget::OnCardClicked()
{
	if (Parent_) Parent_->SelectCharacter(Index_);
}

