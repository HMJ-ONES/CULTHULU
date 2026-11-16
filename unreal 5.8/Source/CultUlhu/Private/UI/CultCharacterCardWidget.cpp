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
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "CultCharacterModel.h"
#include "CultUlhuGameInstance.h"

#include "UI/CultCharacterSelectWidget.h"

#include "UI/CultUIStyle.h"
namespace {
void StyleCard(UButton* Button,bool Selected=false) {
 CultUI::Button(Button,false,Selected); auto Style=Button->GetStyle();
 Style.NormalPadding=FMargin(8,6); Style.PressedPadding=FMargin(8,7,8,5);
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
 const auto& Roster=UCultUlhuGameInstance::Roster();
 UTexture2D* Portrait=nullptr;
 if(Roster.IsValidIndex(Index_)) { auto Path=ACultCharacterModel::PortraitPath(Roster[Index_].Id); if(!Path.IsEmpty()) Portrait=LoadObject<UTexture2D>(nullptr,*Path); }
 if(Roster.IsValidIndex(Index_)) { const auto Credit=ACultCharacterModel::SourceCredit(Roster[Index_].Id); if(!Credit.IsEmpty()) CardButton_->SetToolTipText(FText::FromString(Name+TEXT("\n")+Flavor+TEXT("\n\n")+Credit)); }
 if(Portrait) {
  auto* Image=NewObject<UImage>(V); Image->SetBrushFromTexture(Portrait,true); Image->SetDesiredSizeOverride(FVector2D(76,38));
  auto* ImageFit=NewObject<UScaleBox>(V); ImageFit->SetStretch(EStretch::ScaleToFit); ImageFit->AddChild(Image);
  auto* ImageSize=NewObject<USizeBox>(V); ImageSize->SetWidthOverride(116); ImageSize->SetHeightOverride(34); ImageSize->AddChild(ImageFit); V->AddChildToVerticalBox(ImageSize);
 }
	NameText_ = NewObject<UTextBlock>(V);
	NameText_->SetText(FText::FromString(Short.ToUpper()));
	FSlateFontInfo F = CultUI::Font(18,true);
	NameText_->SetFont(F);
	NameText_->SetColorAndOpacity(FSlateColor(CultUI::Bone));
	NameText_->SetJustification(ETextJustify::Left);
	NameText_->SetAutoWrapText(false);

	UTextBlock* Fl = NewObject<UTextBlock>(V);
	Fl->SetText(FText::FromString(TEXT("MODEL PENDING")));
	FSlateFontInfo F2 = CultUI::Font(11);
	Fl->SetFont(F2);
	Fl->SetColorAndOpacity(FSlateColor(CultUI::Muted));
	Fl->SetJustification(ETextJustify::Left);
	Fl->SetAutoWrapText(true);

	auto* NameSize=NewObject<USizeBox>(V); NameSize->SetWidthOverride(116); NameSize->SetHeightOverride(26);
	auto* Fit=NewObject<UScaleBox>(V); Fit->SetStretch(EStretch::ScaleToFit); Fit->SetStretchDirection(EStretchDirection::DownOnly); NameSize->AddChild(Fit);
	if(auto* S=Cast<UScaleBoxSlot>(Fit->AddChild(NameText_))) { S->SetHorizontalAlignment(HAlign_Left); S->SetVerticalAlignment(VAlign_Center); }
	if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(NameSize)) VS->SetPadding(FMargin(0));
	if(!Portrait) V->AddChildToVerticalBox(Fl);
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

