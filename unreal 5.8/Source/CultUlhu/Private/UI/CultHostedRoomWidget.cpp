#include "UI/CultHostedRoomWidget.h"
#include "UI/CultUIStyle.h"
#include "CultUlhuGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
TSharedRef<SWidget> UCultHostedRoomWidget::RebuildWidget() {
 if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this);
 if(!WidgetTree->RootWidget) {
  auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
  auto* Back=WidgetTree->ConstructWidget<UBorder>(); Back->SetBrushColor(CultUI::Background); auto* S=Root->AddChildToCanvas(Back); S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0));
  auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); auto* Layout=Root->AddChildToCanvas(Box); Layout->SetAnchors(FAnchors(.2,.2,.8,.8)); Layout->SetOffsets(FMargin(0));
  auto Label=[&](FString Text,int Size,FLinearColor C,bool Head=false) { auto* T=CultUI::Label(this,Text,Size,C,Head); T->SetAutoWrapText(true); Box->AddChildToVerticalBox(T)->SetPadding(FMargin(0,0,0,18)); return T; };
  Label(TEXT("ROOM STILL RUNNING"),36,CultUI::Gold,true); Label(TEXT("A bot has taken your place. Your friends can keep playing while this window stays open."),17,CultUI::Bone);
  auto Button=[&](const TCHAR* Text,FName Function) { auto* B=WidgetTree->ConstructWidget<UButton>(); CultUI::Button(B); B->AddChild(CultUI::Label(this,Text,20,CultUI::Bone,true)); FScriptDelegate D; D.BindUFunction(this,Function); B->OnClicked.Add(D); Box->AddChildToVerticalBox(B)->SetPadding(FMargin(0,0,0,10)); };
  Button(TEXT("REJOIN MATCH"),TEXT("Rejoin")); Button(TEXT("COPY ROOM CODE"),TEXT("Copy")); Button(TEXT("CLOSE ROOM & MAIN MENU"),TEXT("Close"));
  Status_=Label(FString(),14,CultUI::Teal);
 }
 return Super::RebuildWidget();
}
void UCultHostedRoomWidget::NativeTick(const FGeometry& G,float Dt) { Super::NativeTick(G,Dt); if(auto* Game=Cast<UCultUlhuGameInstance>(GetGameInstance())) if(Status_) Status_->SetText(FText::FromString(Game->RoomStatus)); }
void UCultHostedRoomWidget::Rejoin() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->RejoinHostedRoom(); }
void UCultHostedRoomWidget::Copy() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->CopyRoomCode(); }
void UCultHostedRoomWidget::Close() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->ReturnToMenu(); }
