#include "UI/CultCharacterSelectWidget.h"
#include "Blueprint/WidgetTree.h"
// CULT-ULHU character select implementation.
#include "UI/CultCharacterSelectWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

#include "UI/CultCharacterCardWidget.h"
#include "CultUlhuGameInstance.h"
#include "CultCanonicalKit.h"

#include "UI/CultUIStyle.h"
#include "UI/CultMenuBackdrop.h"
namespace {
 UTextBlock* MakeText(const FString& Value,float Size,FLinearColor Color) { return CultUI::Label(GetTransientPackage(),Value,Size,Color,Size>=22); }
 UButton* MakeButton(const FString& Label,float Size,bool Primary=false) { auto* B=NewObject<UButton>(); CultUI::Button(B,Primary); B->AddChild(CultUI::Label(B,Label,Size,CultUI::Bone,true,ETextJustify::Center)); return B; }
}

TSharedRef<SWidget> UCultCharacterSelectWidget::RebuildWidget()
{
	if (!bBuilt_)
	{
		bBuilt_ = true;
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UCultCharacterSelectWidget::BuildLayout() {
 Root_=NewObject<UCanvasPanel>(this); auto* Back=NewObject<UCultMenuBackdrop>(this); auto* B=Root_->AddChildToCanvas(Back); B->SetAnchors(FAnchors(0,0,1,1)); B->SetOffsets(FMargin(0));
 auto* Col=NewObject<UVerticalBox>(this); auto* Outer=Root_->AddChildToCanvas(Col); Outer->SetAnchors(FAnchors(.055f,.055f,.945f,.94f)); Outer->SetOffsets(FMargin(0));
 Col->AddChildToVerticalBox(MakeText(UCultUlhuGameInstance::ModeDisplayName(Mode_).ToUpper()+TEXT("  /  ENTITY SELECTION"),13,CultUI::Gold))->SetPadding(FMargin(0,0,0,6));
 TitleText_=MakeText(TEXT("CHOOSE YOUR ENTITY"),44,CultUI::Bone); Col->AddChildToVerticalBox(TitleText_)->SetPadding(FMargin(0,0,0,22));
 auto* Main=NewObject<UHorizontalBox>(this); auto* MainSlot=Col->AddChildToVerticalBox(Main); MainSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); MainSlot->SetPadding(FMargin(0,0,0,20));
 auto* Scroll=NewObject<UScrollBox>(this); Grid_=NewObject<UUniformGridPanel>(this); Grid_->SetSlotPadding(FMargin(0,0,10,10)); Grid_->SetMinDesiredSlotHeight(104); Scroll->AddChild(Grid_); Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
 auto* RosterSlot=Main->AddChildToHorizontalBox(Scroll); RosterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); RosterSlot->SetPadding(FMargin(0,0,24,0));
 auto* Detail=NewObject<UBorder>(this); Detail->SetBrushColor(CultUI::Panel); Detail->SetPadding(FMargin(26)); auto* DetailSize=NewObject<USizeBox>(this); DetailSize->SetWidthOverride(330); DetailSize->AddChild(Detail); Main->AddChildToHorizontalBox(DetailSize);
 auto* DetailCol=NewObject<UVerticalBox>(this); Detail->SetContent(DetailCol);
 DetailCol->AddChildToVerticalBox(MakeText(TEXT("YOUR VESSEL"),13,CultUI::Teal))->SetPadding(FMargin(0,0,0,18));
 DetailText_=MakeText(TEXT(""),16,CultUI::Bone); DetailText_->SetAutoWrapText(true); auto* DetailScroll=NewObject<UScrollBox>(this); DetailScroll->AddChild(DetailText_); DetailCol->AddChildToVerticalBox(DetailScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 DetailCol->AddChildToVerticalBox(MakeText(TEXT("LMB / RMB    PRIMARY ATTACKS\nQ / F / R       ABILITIES\nV                  CHANGE VIEW"),12,CultUI::Muted))->SetPadding(FMargin(0,20,0,0));
 auto* Row=NewObject<UHorizontalBox>(this); BackButton_=MakeButton(TEXT("BACK"),18); BackButton_->OnClicked.AddDynamic(this,&UCultCharacterSelectWidget::OnBackClicked); Row->AddChildToHorizontalBox(BackButton_);
 auto* Space=NewObject<USizeBox>(this); Row->AddChildToHorizontalBox(Space)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 PlayButton_=MakeButton(Mode_==ECultPlayMode::FreeRoam?TEXT("CHOOSE INITIAL BELIEFS"):TEXT("ENTER BATTLE"),22,true); PlayButton_->OnClicked.AddDynamic(this,&UCultCharacterSelectWidget::OnPlayClicked); Row->AddChildToHorizontalBox(PlayButton_); Col->AddChildToVerticalBox(Row);
 if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree")); WidgetTree->RootWidget=Root_;
}

void UCultCharacterSelectWidget::RebuildGrid()
{
	if (!Grid_) BuildLayout();
	Grid_->ClearChildren();
	Cards_.Empty();

	const TArray<FCultRosterEntry>& R = UCultUlhuGameInstance::Roster();
	const int32 Cols = 4;
 if(auto* GI=Cast<UCultUlhuGameInstance>(GetGameInstance())) for(int32 I=0;I<R.Num();++I) if(R[I].Id==GI->SelectedCharacterId) SelectedIndex_=I;
	for (int32 i = 0; i < R.Num(); ++i)
	{
		UCultCharacterCardWidget* Card = CreateWidget<UCultCharacterCardWidget>(
			this, UCultCharacterCardWidget::StaticClass());
		Card->Setup(i, R[i].DisplayName, R[i].Flavor, this);
		auto* Cell=Grid_->AddChildToUniformGrid(Card, i / Cols, i % Cols); Cell->SetHorizontalAlignment(HAlign_Fill); Cell->SetVerticalAlignment(VAlign_Fill);
		Cards_.Add(Card);
	}
	RefreshSelection();
}

void UCultCharacterSelectWidget::SelectCharacter(int32 Index)
{
	const TArray<FCultRosterEntry>& R = UCultUlhuGameInstance::Roster();
	if (Index < 0 || Index >= R.Num()) return;
	SelectedIndex_ = Index;
	if (auto* GI = Cast<UCultUlhuGameInstance>(GetGameInstance()))
		GI->SelectedCharacterId = R[Index].Id;
	RefreshSelection();
}

void UCultCharacterSelectWidget::RefreshSelection()
{
	const TArray<FCultRosterEntry>& R = UCultUlhuGameInstance::Roster();
	for (int32 i = 0; i < Cards_.Num(); ++i)
		Cards_[i]->SetSelected(i == SelectedIndex_);
	if (DetailText_ && R.IsValidIndex(SelectedIndex_))
	{
		const FCultRosterEntry& E = R[SelectedIndex_];
		FString Kit=CultCanonicalKit::Summary(E.Id); cultulhu::CharacterDef Def;
  if(CultCanonicalKit::Load(E.Id,Def)&&Mode_!=ECultPlayMode::FreeRoam) {
   Kit.ReplaceInline(*FString::Printf(TEXT("HP %.0f"),Def.maxHp),TEXT("HP 100"));
   if(Mode_==ECultPlayMode::Dotgoo&&E.Id==TEXT("cthulhu_avatar")) Kit.ReplaceInline(*FString(TEXT("RMB: ")+FString(UTF8_TO_TCHAR(Def.rightClick.name.c_str()))),TEXT("RMB: Minion Sling (20s cooldown)"));
  }
  DetailText_->SetText(FText::FromString(E.DisplayName.ToUpper()+FString::Printf(TEXT("\n%.0f m  /  ELDRITCH ENTITY\n\n"),E.HeightM)+Kit));
	}

}

void UCultCharacterSelectWidget::OnBackClicked()
{
	if (auto* GI = Cast<UCultUlhuGameInstance>(GetGameInstance())) GI->ShowHomeScreen();
}

void UCultCharacterSelectWidget::OnPlayClicked()
{
	if (auto* GI = Cast<UCultUlhuGameInstance>(GetGameInstance())) GI->OpenBeliefSelect();
}

