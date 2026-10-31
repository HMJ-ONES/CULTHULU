#include "UI/CultFreeRoamMenuWidget.h"
#include "UI/CultUIStyle.h"
#include "CultUlhuGameInstance.h"
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
#include "Brushes/SlateRoundedBoxBrush.h"

TSharedRef<SWidget> UCultFreeRoamMenuWidget::RebuildWidget() {
 if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
 if(!WidgetTree->RootWidget) {
  auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance());
  auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
  auto* Shade=WidgetTree->ConstructWidget<UBorder>(); Shade->SetBrushColor(FLinearColor(.006f,.01f,.018f,.95f)); auto* Back=Root->AddChildToCanvas(Shade); Back->SetAnchors(FAnchors(0,0,1,1)); Back->SetOffsets(FMargin(0));
  auto* Bounds=WidgetTree->ConstructWidget<USizeBox>(); Bounds->SetWidthOverride(600);
  auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Bounds->AddChild(Box);
  auto* Layout=Root->AddChildToCanvas(Bounds); Layout->SetAnchors(FAnchors(.5f,.5f)); Layout->SetAlignment(FVector2D(.5,.5)); Layout->SetAutoSize(true);
  Box->AddChildToVerticalBox(CultUI::Label(this,TEXT("THE DARK WILDS"),14,CultUI::Gold))->SetPadding(FMargin(0,0,0,6));
  Box->AddChildToVerticalBox(CultUI::Label(this,TEXT("FREE ROAM"),42,CultUI::Bone,true))->SetPadding(FMargin(0,0,0,14));
  auto Button=[&](const TCHAR* Label,FName Function,bool Enabled=true) { auto* B=WidgetTree->ConstructWidget<UButton>(); CultUI::Button(B); B->AddChild(CultUI::Label(this,Label,18,CultUI::Bone,true)); FScriptDelegate Click; Click.BindUFunction(this,Function); B->OnClicked.Add(Click); B->SetIsEnabled(Enabled); Box->AddChildToVerticalBox(B)->SetPadding(FMargin(0,0,0,7)); };
  if(G&&G->FreeRoamReady) { Button(TEXT("RESUME"),TEXT("Resume")); Button(TEXT("SAVE WORLD"),TEXT("Save"),GetWorld()->GetNetMode()!=NM_Client); }
  Button(TEXT("CONTINUE SAVED WORLD"),TEXT("Load"),G&&G->HasFreeRoamSave());
  if(!G||!G->FreeRoamReady) Button(TEXT("NEW WORLD"),TEXT("NewGame"));
  Button(TEXT("HOST CO-OP  /  LAN + DIRECT IP"),TEXT("Host"),G&&G->FreeRoamReady&&GetWorld()->GetNetMode()==NM_Standalone);
  Address_=WidgetTree->ConstructWidget<UEditableTextBox>();
  FEditableTextBoxStyle InputStyle=Address_->GetWidgetStyle(); InputStyle.SetFont(CultUI::Font(16)); InputStyle.SetForegroundColor(CultUI::Bone); InputStyle.SetBackgroundColor(FLinearColor::White); InputStyle.SetPadding(FMargin(12,8));
  InputStyle.SetBackgroundImageNormal(FSlateRoundedBoxBrush(CultUI::Panel,2.f,CultUI::Line,1.f)); InputStyle.SetBackgroundImageHovered(FSlateRoundedBoxBrush(CultUI::Panel,2.f,CultUI::Teal,1.f)); InputStyle.SetBackgroundImageFocused(FSlateRoundedBoxBrush(CultUI::Panel,2.f,CultUI::Gold,1.f)); Address_->SetWidgetStyle(InputStyle); Address_->SetHintText(FText::FromString(TEXT("Host address, e.g. 192.168.1.12:7777"))); Box->AddChildToVerticalBox(Address_)->SetPadding(FMargin(0,7,0,7));
  Button(TEXT("JOIN CO-OP"),TEXT("Join"));
  Button(G&&G->FreeRoamReady?(GetWorld()->GetNetMode()==NM_Client?TEXT("LEAVE CO-OP"):TEXT("SAVE & RETURN TO MAIN MENU")):TEXT("BACK"),TEXT("Exit"));
  Box->AddChildToVerticalBox(CultUI::Label(this,TEXT("Solo play pauses here. Co-op keeps running. The host saves the world."),12,CultUI::Muted))->SetPadding(FMargin(0,9,0,5));
  Status_=CultUI::Label(this,G?G->SaveStatus:FString(),13,CultUI::Teal); Status_->SetAutoWrapText(true); Box->AddChildToVerticalBox(Status_);
 }
 return Super::RebuildWidget();
}
FReply UCultFreeRoamMenuWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E) { if(E.GetKey()==EKeys::Escape) { Resume(); return FReply::Handled(); } return Super::NativeOnKeyDown(G,E); }
void UCultFreeRoamMenuWidget::Status() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) if(Status_) Status_->SetText(FText::FromString(G->SaveStatus)); }
void UCultFreeRoamMenuWidget::Resume() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) { if(G->FreeRoamReady) G->ResumeFreeRoam(); else { G->FreeRoamMenuOpen=false; G->ShowHomeScreen(); } } }
void UCultFreeRoamMenuWidget::Save() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->SaveFreeRoam(); Status(); }
void UCultFreeRoamMenuWidget::Load() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->ContinueFreeRoam(); Status(); }
void UCultFreeRoamMenuWidget::NewGame() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) { G->FreeRoamMenuOpen=false; G->OpenCharacterSelect(ECultPlayMode::FreeRoam); } }
void UCultFreeRoamMenuWidget::Host() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->HostFreeRoam(); Status(); }
void UCultFreeRoamMenuWidget::Join() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->JoinFreeRoam(Address_?Address_->GetText().ToString():FString()); Status(); }
void UCultFreeRoamMenuWidget::Exit() { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) { if(G->FreeRoamReady) G->ExitFreeRoam(); else Resume(); } Status(); }
