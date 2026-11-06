#include "UI/CultRoomMenuWidget.h"
#include "UI/CultUIStyle.h"
#include "CultUlhuGameInstance.h"
#include "CultLobbySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Brushes/SlateRoundedBoxBrush.h"
TSharedRef<SWidget> UCultRoomMenuWidget::RebuildWidget() {
 if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this);
 if(!WidgetTree->RootWidget) {
  auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance()); auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
  auto* Shade=WidgetTree->ConstructWidget<UBorder>(); Shade->SetBrushColor(CultUI::Background); auto* S=Root->AddChildToCanvas(Shade); S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0));
  auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); auto* Layout=Root->AddChildToCanvas(Scroll); Layout->SetAnchors(FAnchors(.22f,.07f,.78f,.93f)); Layout->SetOffsets(FMargin(0));
  auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(Box);
  auto Label=[&](FString Text,int Size,FLinearColor Color,bool Head=false) { auto* T=CultUI::Label(this,Text,Size,Color,Head); T->SetAutoWrapText(true); Box->AddChildToVerticalBox(T)->SetPadding(FMargin(0,0,0,12)); return T; };
  Label(G?UCultUlhuGameInstance::ModeDisplayName(G->SelectedMode):TEXT("PLAY"),36,CultUI::Gold,true);
  Label(TEXT("PLAY TOGETHER"),14,CultUI::Teal);
  Label(TEXT("Join the same Radmin network. Choose your entity, then connect."),14,CultUI::Muted);
  auto Button=[&](const TCHAR* Text,FName Function,bool Primary=false,bool Enabled=true) { auto* B=WidgetTree->ConstructWidget<UButton>(); CultUI::Button(B,Primary); B->AddChild(CultUI::Label(this,Text,20,CultUI::Bone,true)); FScriptDelegate D; D.BindUFunction(this,Function); B->OnClicked.Add(D); B->SetIsEnabled(Enabled); Box->AddChildToVerticalBox(B)->SetPadding(FMargin(0,0,0,8)); };
  Button(TEXT("ENTER MATCH"),TEXT("Find"),true);
  Label(TEXT("HOSTING · use your existing Radmin network"),13,CultUI::Muted);
  Network_=WidgetTree->ConstructWidget<UEditableTextBox>(); Network_->SetText(FText::FromString(TEXT("CULT-ULHU-PUBLIC"))); Network_->SetHintText(FText::FromString(TEXT("Radmin network name"))); Box->AddChildToVerticalBox(Network_)->SetPadding(FMargin(0,0,0,6));
  Password_=WidgetTree->ConstructWidget<UEditableTextBox>(); Password_->SetIsPassword(true); Password_->SetHintText(FText::FromString(TEXT("Radmin network password · memory only"))); Box->AddChildToVerticalBox(Password_)->SetPadding(FMargin(0,0,0,8));
  Button(TEXT("HOST GAME"),TEXT("Host")); Button(TEXT("PRIVATE ROOM"),TEXT("Private"));
  Code_=WidgetTree->ConstructWidget<UEditableTextBox>(); Code_->SetHintText(FText::FromString(TEXT("Paste your friend's room code"))); auto Style=Code_->GetWidgetStyle(); Style.SetFont(CultUI::Font(16)); Style.SetForegroundColor(CultUI::Bone); Style.SetBackgroundImageNormal(FSlateRoundedBoxBrush(CultUI::Panel,2.f,CultUI::Line,1.f)); Style.SetBackgroundImageHovered(FSlateRoundedBoxBrush(CultUI::Panel,2.f,CultUI::Teal,1.f)); Style.SetBackgroundImageFocused(FSlateRoundedBoxBrush(CultUI::Panel,2.f,CultUI::Gold,1.f)); Code_->SetWidgetStyle(Style); Box->AddChildToVerticalBox(Code_)->SetPadding(FMargin(0,10,0,8));
  Button(TEXT("ENTER ROOM CODE"),TEXT("Join"));
  Network_->SetWidgetStyle(Style); Password_->SetWidgetStyle(Style);
  Status_=Label(G?G->RoomStatus:FString(),14,CultUI::Teal);
  Connection_=Label(FString(),14,CultUI::Bone);
  Connect_=WidgetTree->ConstructWidget<UButton>(); CultUI::Button(Connect_,true); Connect_->AddChild(CultUI::Label(this,TEXT("I JOINED THE RADMIN NETWORK · CONNECT"),16,CultUI::Bone,true)); Connect_->OnClicked.AddDynamic(this,&UCultRoomMenuWidget::Connect); Box->AddChildToVerticalBox(Connect_)->SetPadding(FMargin(0,0,0,8)); Connect_->SetVisibility(ESlateVisibility::Collapsed);
  if(G&&G->SelectedMode==ECultPlayMode::FreeRoam) Button(TEXT("CONTINUE SAVED WORLD"),TEXT("Continue"),false,G->HasFreeRoamSave());
  Button(G&&G->SelectedMode==ECultPlayMode::FreeRoam?TEXT("NEW WORLD / OFFLINE"):TEXT("PRACTICE / OFFLINE"),TEXT("Offline")); Button(TEXT("BACK"),TEXT("Back"));
 }
 return Super::RebuildWidget();
}
void UCultRoomMenuWidget::NativeTick(const FGeometry& G,float Dt) { Super::NativeTick(G,Dt); if(auto* Game=Cast<UCultUlhuGameInstance>(GetGameInstance())) {
 if(Status_) Status_->SetText(FText::FromString(Game->RoomStatus));
 if(auto* L=Game->GetSubsystem<UCultLobbySubsystem>()) {
  const bool Waiting=L->State==ECultLobbyState::WaitingForRadmin;
  if(Connect_) Connect_->SetVisibility(Waiting?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
  if(Connection_) { Connection_->SetVisibility(Waiting?ESlateVisibility::Visible:ESlateVisibility::Collapsed); Connection_->SetText(FText::FromString(Waiting?TEXT("Radmin → Join network\nName: ")+L->RequiredNetwork+TEXT("\nPassword: ")+L->RequiredNetworkPassword:FString())); }
 }
} }
void UCultRoomMenuWidget::Find() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->BeginRoomAction(ECultRoomAction::Find); }
void UCultRoomMenuWidget::Host() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) if(auto* L=G->GetSubsystem<UCultLobbySubsystem>()) if(L->ConfigureRadminNetwork(Network_->GetText().ToString(),Password_->GetText().ToString())) G->BeginRoomAction(ECultRoomAction::Host); }
void UCultRoomMenuWidget::Private() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) if(auto* L=G->GetSubsystem<UCultLobbySubsystem>()) if(L->ConfigureRadminNetwork(Network_->GetText().ToString(),Password_->GetText().ToString())) G->BeginRoomAction(ECultRoomAction::Private); }
void UCultRoomMenuWidget::Connect() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) if(auto* L=G->GetSubsystem<UCultLobbySubsystem>()) L->ContinueRadminConnection(); }
void UCultRoomMenuWidget::Join() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->BeginRoomAction(ECultRoomAction::Join,Code_?Code_->GetText().ToString():FString()); }
void UCultRoomMenuWidget::Offline() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->BeginRoomAction(ECultRoomAction::None); }
void UCultRoomMenuWidget::Continue() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->ContinueFreeRoam(); }
void UCultRoomMenuWidget::Back() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->ShowHomeScreen(); }
