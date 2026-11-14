#include "UI/CultBeliefSelectWidget.h"
#include "UI/CultUIStyle.h"
#include "CultUlhuGameInstance.h"
#include "CultBeliefCatalog.h"
#include "beliefs/Belief.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"

namespace {
UTextBlock* Text(UObject* Outer,const FString& Value,int Size) {
 auto* T=CultUI::Label(Outer,Value,Size,CultUI::Bone,Size>=22); T->SetAutoWrapText(true); return T;
}
}
void UCultBeliefChoiceButton::Setup(int32 Id,UCultBeliefSelectWidget* Owner) { Id_=Id; Owner_=Owner; OnClicked.AddDynamic(this,&UCultBeliefChoiceButton::Choose); }
void UCultBeliefChoiceButton::Choose() { if(Owner_) Owner_->ToggleChoice(Id_); }
TSharedRef<SWidget> UCultBeliefSelectWidget::RebuildWidget()
{
 if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
 if(!WidgetTree->RootWidget) {
  auto* Backdrop=NewObject<UBorder>(this); Backdrop->SetBrushColor(CultUI::Background); Backdrop->SetPadding(FMargin(64,36)); WidgetTree->RootWidget=Backdrop;
  auto* Col=NewObject<UVerticalBox>(this); Backdrop->SetContent(Col);
  
  Col->AddChildToVerticalBox(Text(this,TEXT("SHAPE YOUR DEVOTION"),40));
  Col->AddChildToVerticalBox(Text(this,TEXT("Beliefs can be changed by pressing B. Followers take time to adopt changes (normally 120 seconds)."),15))->SetPadding(FMargin(0,10));
  Count_=Text(this,TEXT(""),22); Col->AddChildToVerticalBox(Count_)->SetPadding(FMargin(0,6));
  auto* Grid=NewObject<UUniformGridPanel>(this); Grid->SetSlotPadding(FMargin(6)); Grid->SetMinDesiredSlotHeight(56); Col->AddChildToVerticalBox(Grid);
  for(int I=0;I<12;++I) {
   auto* B=NewObject<UCultBeliefChoiceButton>(this); B->Setup(I,this); CultUI::Button(B); auto* Label=Text(this,UTF8_TO_TCHAR(cultulhu::beliefName(static_cast<cultulhu::Belief>(I))),17); Label->SetAutoWrapText(false); B->AddChild(Label);
   auto* Cell=Grid->AddChildToUniformGrid(B,I/4,I%4); Cell->SetHorizontalAlignment(HAlign_Fill); Cell->SetVerticalAlignment(VAlign_Fill); Buttons_.Add(B);
  }
  Detail_=Text(this,TEXT("Select a belief to read its original rules. Conversion gives a ready three-person recruitment burst."),17);
  auto* Scroll=NewObject<UScrollBox>(this); Scroll->AddChild(Detail_); Col->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  Enter_=NewObject<UButton>(this); auto* EnterLabel=Text(this,TEXT("ENTER THE WILDS"),24); EnterLabel->SetAutoWrapText(false); Enter_->AddChild(EnterLabel);
  CultUI::Button(Enter_,true);
  Enter_->OnClicked.AddDynamic(this,&UCultBeliefSelectWidget::Enter);
  auto* EnterSize=NewObject<USizeBox>(this); EnterSize->SetWidthOverride(420); EnterSize->SetHeightOverride(56); EnterSize->AddChild(Enter_);
  auto* EnterCell=Col->AddChildToVerticalBox(EnterSize); EnterCell->SetPadding(FMargin(0,12)); EnterCell->SetHorizontalAlignment(HAlign_Center);
  auto* BackButton=NewObject<UButton>(this); auto* BackLabel=Text(this,TEXT("BACK TO CHARACTERS"),18); BackLabel->SetAutoWrapText(false); BackButton->AddChild(BackLabel);
  CultUI::Button(BackButton);
  BackButton->OnClicked.AddDynamic(this,&UCultBeliefSelectWidget::Back); Col->AddChildToVerticalBox(BackButton)->SetHorizontalAlignment(HAlign_Center);
  Refresh();
 }
 return Super::RebuildWidget();
}
void UCultBeliefSelectWidget::ToggleChoice(int32 Id)
{
 if(Id<0||Id>=12) return;
 if(Choices_.Contains(Id)) Choices_.Remove(Id); else if(Choices_.Num()<3) Choices_.Add(Id);
 if(Detail_) Detail_->SetText(FText::FromString(CultFaith::Description(Id)));
 Refresh();
}
void UCultBeliefSelectWidget::Refresh()
{
 if(Count_) Count_->SetText(FText::FromString(FString::Printf(TEXT("%d / 3 selected — choose any three"),Choices_.Num())));
 if(Enter_) Enter_->SetIsEnabled(CanEnter());
 for(int I=0;I<Buttons_.Num();++I) {
  const bool Chosen=Choices_.Contains(I); CultUI::Button(Buttons_[I],false,Chosen);
  if(auto* Label=Cast<UTextBlock>(Buttons_[I]->GetChildAt(0))) Label->SetText(FText::FromString((Chosen?TEXT("+  "):TEXT(""))+FString(UTF8_TO_TCHAR(cultulhu::beliefName(static_cast<cultulhu::Belief>(I))))));
 }
}
void UCultBeliefSelectWidget::Enter() { if(auto* GI=Cast<UCultUlhuGameInstance>(GetGameInstance())) if(GI->SetInitialBeliefs(Choices_)) GI->StartPlay(); }
void UCultBeliefSelectWidget::Back() { if(auto* GI=Cast<UCultUlhuGameInstance>(GetGameInstance())) GI->OpenCharacterSelect(GI->SelectedMode); }
