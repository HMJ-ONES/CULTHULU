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
#include "Components/ScaleBox.h"

#include "UI/CultCharacterCardWidget.h"
#include "UI/CultCharacterPreviewWidget.h"
#include "CultUlhuGameInstance.h"
#include "CultCanonicalKit.h"

#include "UI/CultUIStyle.h"
#include "UI/CultMenuBackdrop.h"
namespace {
 UTextBlock* MakeText(const FString& Value,float Size,FLinearColor Color) { return CultUI::Label(GetTransientPackage(),Value,Size,Color,Size>=22); }
 UButton* MakeButton(const FString& Label,float Size,bool Primary=false) { auto* B=NewObject<UButton>(); CultUI::Button(B,Primary); B->SetColorAndOpacity(FLinearColor::White); B->AddChild(CultUI::Label(B,Label,Size,CultUI::Bone,true,ETextJustify::Center)); return B; }
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
 // A fitted reference canvas cancels the inherited DPI curve without shrinking
 // fixed-width kit details to a tiny fraction of a wide editor viewport.
 auto* Fit=NewObject<UScaleBox>(this); Fit->SetStretch(EStretch::ScaleToFit);
 auto* FitSlot=Root_->AddChildToCanvas(Fit); FitSlot->SetAnchors(FAnchors(0,0,1,1)); FitSlot->SetOffsets(FMargin(0));
 auto* Reference=NewObject<USizeBox>(this); Reference->SetWidthOverride(1280); Reference->SetHeightOverride(720); Fit->AddChild(Reference);
 auto* Content=NewObject<UCanvasPanel>(this); Reference->AddChild(Content);
 auto* Col=NewObject<UVerticalBox>(this); auto* Outer=Content->AddChildToCanvas(Col); Outer->SetAnchors(FAnchors(.025f,.025f,.975f,.975f)); Outer->SetOffsets(FMargin(0));
 Col->AddChildToVerticalBox(MakeText(UCultUlhuGameInstance::ModeDisplayName(Mode_).ToUpper()+TEXT("  /  ENTITY SELECTION"),13,CultUI::Gold))->SetPadding(FMargin(0,0,0,6));
 TitleText_=MakeText(TEXT("CHOOSE YOUR ENTITY"),28,CultUI::Bone); Col->AddChildToVerticalBox(TitleText_)->SetPadding(FMargin(0,0,0,8));
 auto* Main=NewObject<UHorizontalBox>(this); auto* MainSlot=Col->AddChildToVerticalBox(Main); MainSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); MainSlot->SetPadding(FMargin(0,0,0,12));
 auto* Scroll=NewObject<UScrollBox>(this); Grid_=NewObject<UUniformGridPanel>(this); Grid_->SetSlotPadding(FMargin(0,0,8,8)); Grid_->SetMinDesiredSlotHeight(76); Scroll->AddChild(Grid_); Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
 auto* RosterSize=NewObject<USizeBox>(this); RosterSize->SetHeightOverride(172); RosterSize->AddChild(Scroll);
 Preview_=CreateWidget<UCultCharacterPreviewWidget>(this,UCultCharacterPreviewWidget::StaticClass());
 auto* Studio=NewObject<UBorder>(this); Studio->SetBrushColor(CultUI::Panel); Studio->SetPadding(FMargin(0)); Studio->SetContent(Preview_);
 auto* StudioSlot=Main->AddChildToHorizontalBox(Studio); StudioSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); StudioSlot->SetPadding(FMargin(0,0,24,0));
 auto* Detail=NewObject<UBorder>(this); Detail->SetBrushColor(CultUI::Panel); Detail->SetPadding(FMargin(20,12)); auto* DetailSize=NewObject<USizeBox>(this); DetailSize->SetWidthOverride(490); DetailSize->AddChild(Detail); Main->AddChildToHorizontalBox(DetailSize);
 auto* DetailScroll=NewObject<UScrollBox>(this); Detail->SetContent(DetailScroll); auto* DetailCol=NewObject<UVerticalBox>(this); DetailScroll->AddChild(DetailCol);
 DetailText_=MakeText(TEXT(""),24,CultUI::Bone); DetailText_->SetAutoWrapText(true); DetailCol->AddChildToVerticalBox(DetailText_)->SetPadding(FMargin(0,0,0,2));
 DetailSubtitle_=MakeText(TEXT(""),12,CultUI::Gold); DetailSubtitle_->SetAutoWrapText(true); DetailCol->AddChildToVerticalBox(DetailSubtitle_)->SetPadding(FMargin(0,0,0,4));
 DetailFlavor_=MakeText(TEXT(""),13,CultUI::Muted); DetailFlavor_->SetAutoWrapText(true); DetailCol->AddChildToVerticalBox(DetailFlavor_)->SetPadding(FMargin(0,0,0,8));
 DetailFlavor_->SetVisibility(ESlateVisibility::Collapsed);
 DetailStats_=MakeText(TEXT(""),12,CultUI::Teal); DetailCol->AddChildToVerticalBox(DetailStats_)->SetPadding(FMargin(0,0,0,6));
 const TCHAR* Keys[]={TEXT("Q"),TEXT("F"),TEXT("R"),TEXT("RMB")};
 for(int I=0;I<4;++I) {
  auto* Row=NewObject<UHorizontalBox>(this); auto* Key=NewObject<UBorder>(this); Key->SetBrushColor(FLinearColor(.025f,.075f,.075f)); Key->SetPadding(FMargin(4)); Key->SetContent(CultUI::Label(this,Keys[I],I==3?12:16,CultUI::Teal,true,ETextJustify::Center));
  auto* KeySize=NewObject<USizeBox>(this); KeySize->SetWidthOverride(42); KeySize->SetHeightOverride(38); KeySize->AddChild(Key); Row->AddChildToHorizontalBox(KeySize)->SetPadding(FMargin(0,0,12,0));
  auto* Text=NewObject<UVerticalBox>(this); auto* Name=CultUI::Label(this,TEXT(""),16,CultUI::Bone,true); Name->SetAutoWrapText(true); Text->AddChildToVerticalBox(Name)->SetPadding(FMargin(0,0,0,3)); AbilityNames_.Add(Name);
  auto* Meta=MakeText(TEXT(""),11,CultUI::Muted); Meta->SetAutoWrapText(true); Text->AddChildToVerticalBox(Meta); AbilityMeta_.Add(Meta);
  Row->AddChildToHorizontalBox(Text)->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); DetailCol->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,0,0,4));
 }
 Col->AddChildToVerticalBox(RosterSize)->SetPadding(FMargin(0,0,0,12));
 auto* Hints=NewObject<UHorizontalBox>(this); ModeHint_=MakeText(TEXT(""),11,CultUI::Gold); ModeHint_->SetAutoWrapText(true); Hints->AddChildToHorizontalBox(ModeHint_)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 Hints->AddChildToHorizontalBox(MakeText(TEXT("LMB melee / V view / Tap Shift dash / Hold Shift sprint"),10,CultUI::Muted))->SetPadding(FMargin(16,0,0,0)); Col->AddChildToVerticalBox(Hints)->SetPadding(FMargin(0,0,0,8));
 auto* Row=NewObject<UHorizontalBox>(this); BackButton_=MakeButton(TEXT("BACK"),18); BackButton_->OnClicked.AddDynamic(this,&UCultCharacterSelectWidget::OnBackClicked); Row->AddChildToHorizontalBox(BackButton_);
 auto* Space=NewObject<USizeBox>(this); Row->AddChildToHorizontalBox(Space)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 PlayButton_=MakeButton((Mode_==ECultPlayMode::FreeRoam&&Cast<UCultUlhuGameInstance>(GetGameInstance())->RoomAction!=ECultRoomAction::Join)?TEXT("CHOOSE INITIAL BELIEFS"):TEXT("ENTER BATTLE"),22,true); PlayButton_->OnClicked.AddDynamic(this,&UCultCharacterSelectWidget::OnPlayClicked); Row->AddChildToHorizontalBox(PlayButton_); Col->AddChildToVerticalBox(Row);
 if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree")); WidgetTree->RootWidget=Root_;
}

void UCultCharacterSelectWidget::RebuildGrid()
{
	if (!Grid_) BuildLayout();
	Grid_->ClearChildren();
	Cards_.Empty();

	const TArray<FCultRosterEntry>& R = UCultUlhuGameInstance::Roster();
	const int32 Cols = 8;
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
  if(Preview_) Preview_->ShowCharacter(E.Id);
  FString Short=E.DisplayName,Subtitle; int32 Comma; if(E.DisplayName.FindChar(',',Comma)) { Short=E.DisplayName.Left(Comma); Subtitle=E.DisplayName.Mid(Comma+1).TrimStartAndEnd(); }
  DetailText_->SetText(FText::FromString(Short.ToUpper())); DetailSubtitle_->SetText(FText::FromString(Subtitle)); DetailFlavor_->SetText(FText::FromString(E.Flavor));
  cultulhu::CharacterDef Def;
  if(CultCanonicalKit::Load(E.Id,Def)) {
   DetailStats_->SetText(FText::FromString(FString::Printf(TEXT("%.0f M / HP %.0f / STAMINA %.0f"),E.HeightM,Mode_==ECultPlayMode::FreeRoam?Def.maxHp:100.f,Def.maxStamina)));
   const cultulhu::SpellDef* Spells[]={&Def.qAbility,&Def.fAbility,&Def.rAbility};
   for(int I=0;I<3;++I) {
    const auto& Spell=*Spells[I]; AbilityNames_[I]->SetText(FText::FromString(UTF8_TO_TCHAR(Spell.name.c_str())));
    FString Kind=UTF8_TO_TCHAR(Spell.effectKind.c_str()); Kind.ReplaceInline(TEXT("_"),TEXT(" ")); if(Kind==TEXT("aoe damage")) Kind=TEXT("area damage");
    if(Mode_==ECultPlayMode::Domination&&Kind==TEXT("summon")) Kind=TEXT("area strike");
    if(Mode_!=ECultPlayMode::FreeRoam&&Kind==TEXT("heal")) Kind=TEXT("heal allied heroes");
    AbilityMeta_[I]->SetText(FText::FromString(Kind.ToUpper()+FString::Printf(TEXT(" / %gs / %g STAMINA"),double(Spell.cooldownSec),double(Spell.staminaCost))));
   }
   const bool Sling=Mode_==ECultPlayMode::Dotgoo&&E.Id==TEXT("cthulhu_avatar");
   AbilityNames_[3]->SetText(FText::FromString(Sling?TEXT("Minion Sling"):UTF8_TO_TCHAR(Def.rightClick.name.c_str())));
   AbilityMeta_[3]->SetText(FText::FromString(Sling?TEXT("LIFT / THROW / 20s COOLDOWN"):TEXT("ALTERNATE ATTACK")));
  }
  ModeHint_->SetText(FText::FromString(Mode_==ECultPlayMode::FreeRoam?(Cast<UCultUlhuGameInstance>(GetGameInstance())->RoomAction==ECultRoomAction::Join?TEXT("Join your friend. Their world and cult beliefs are shared."):TEXT("Choose 3 starting beliefs next. Press B in play to change them over time.")):Mode_==ECultPlayMode::Dotgoo?TEXT("Choose Q or F at level 1. Both unlock at level 3; R at level 6."):TEXT("Hold the seal. Eliminations add points.")));
	}

}

void UCultCharacterSelectWidget::OnBackClicked()
{
	if (auto* GI = Cast<UCultUlhuGameInstance>(GetGameInstance())) GI->OpenModeEntry(Mode_);
}

void UCultCharacterSelectWidget::OnPlayClicked()
{
	if (auto* GI = Cast<UCultUlhuGameInstance>(GetGameInstance())) GI->OpenBeliefSelect();
}

