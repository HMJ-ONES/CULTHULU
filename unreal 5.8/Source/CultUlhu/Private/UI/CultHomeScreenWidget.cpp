#include "UI/CultHomeScreenWidget.h"
#include "UI/CultUIStyle.h"
#include "UI/CultMenuBackdrop.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "CultUlhuGameInstance.h"
TSharedRef<SWidget> UCultHomeScreenWidget::RebuildWidget() { if(!bBuilt_) { bBuilt_=true; BuildLayout(); } return Super::RebuildWidget(); }
void UCultHomeScreenWidget::BuildLayout() {
 Root_=NewObject<UCanvasPanel>(this);
 auto* Back=NewObject<UCultMenuBackdrop>(this); auto* BS=Root_->AddChildToCanvas(Back); BS->SetAnchors(FAnchors(0,0,1,1)); BS->SetOffsets(FMargin(0));
 ButtonBox_=NewObject<UVerticalBox>(this);
 auto* Bounds=NewObject<USizeBox>(this); Bounds->SetWidthOverride(590); Bounds->AddChild(ButtonBox_);
 auto* LayoutSlot=Root_->AddChildToCanvas(Bounds); LayoutSlot->SetAnchors(FAnchors(.075f,.48f)); LayoutSlot->SetAlignment(FVector2D(0,.5)); LayoutSlot->SetAutoSize(true);
 auto Add=[&](const FString& Value,int Size,FLinearColor Color,bool Head,float Bottom) { auto* T=CultUI::Label(this,Value,Size,Color,Head); ButtonBox_->AddChildToVerticalBox(T)->SetPadding(FMargin(0,0,0,Bottom)); };
 Add(TEXT("AN AGE OF GODS AND DEVOTION"),12,CultUI::Gold,false,8);
 Add(TEXT("CULT-ULHU"),76,CultUI::Bone,true,2);
 Add(TEXT("The world will learn your name."),17,CultUI::Muted,false,36);
 Add(TEXT("CHOOSE YOUR CONFLICT"),12,CultUI::Teal,false,12);
 for(auto* B:{MakeModeButton(TEXT("FREE ROAM"),TEXT("Raise a cult. Shape its beliefs. Claim the Dark Wilds.")),MakeModeButton(TEXT("DOMINATION"),TEXT("5v5  /  Control the seal. Every elimination counts.")),MakeModeButton(TEXT("DEFENSE OF THE GREAT OLD ONES"),TEXT("5v5  /  Three lanes. One rival god to overthrow."))}) ButtonBox_->AddChildToVerticalBox(B)->SetPadding(FMargin(0,0,0,10));
 auto* Foot=CultUI::Label(this,TEXT("CULT-ULHU    /    THE DARK WILDS"),11,CultUI::Muted); auto* FS=Root_->AddChildToCanvas(Foot); FS->SetAnchors(FAnchors(.075f,.947f)); FS->SetAutoSize(true);
 auto* Verse=CultUI::Label(this,TEXT("THE GREAT OLD ONES ARE LISTENING"),11,CultUI::Gold); auto* VS=Root_->AddChildToCanvas(Verse); VS->SetAnchors(FAnchors(.77f,.84f)); VS->SetAlignment(FVector2D(.5f,0)); VS->SetAutoSize(true);
 if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree")); WidgetTree->RootWidget=Root_;
}
UButton* UCultHomeScreenWidget::MakeModeButton(const FString& Title,const FString& Sub) {
 auto* B=NewObject<UButton>(this); CultUI::Button(B); B->SetToolTipText(FText::FromString(Sub));
 auto* V=NewObject<UVerticalBox>(B); auto* Heading=CultUI::Label(B,Title,26,CultUI::Bone,true); auto* Body=CultUI::Label(B,Sub,13,CultUI::Muted);
 V->AddChildToVerticalBox(Heading)->SetPadding(FMargin(6,3,6,4)); V->AddChildToVerticalBox(Body)->SetPadding(FMargin(6,0,6,5)); if(auto* ContentSlot=Cast<UButtonSlot>(B->AddChild(V))) ContentSlot->SetHorizontalAlignment(HAlign_Fill);
 if(Title==TEXT("FREE ROAM")) B->OnClicked.AddDynamic(this,&UCultHomeScreenWidget::OnFreeRoamClicked);
 else if(Title==TEXT("DOMINATION")) B->OnClicked.AddDynamic(this,&UCultHomeScreenWidget::OnDominationClicked);
 else B->OnClicked.AddDynamic(this,&UCultHomeScreenWidget::OnDotgooClicked); return B;
}
void UCultHomeScreenWidget::OnFreeRoamClicked() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->OpenModeEntry(ECultPlayMode::FreeRoam); }
void UCultHomeScreenWidget::OnDominationClicked() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->OpenModeEntry(ECultPlayMode::Domination); }
void UCultHomeScreenWidget::OnDotgooClicked() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->OpenModeEntry(ECultPlayMode::Dotgoo); }
