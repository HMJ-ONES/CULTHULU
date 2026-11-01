#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "UI/CultRoomMenuWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
namespace {
bool RoomClick(UUserWidget* W,const TCHAR* Text) {
 if(!W||!W->WidgetTree) return false; TArray<UWidget*> Children; W->WidgetTree->GetAllWidgets(Children);
 for(auto* C:Children) if(auto* T=Cast<UTextBlock>(C)) if(T->GetText().ToString()==Text) for(auto* Parent=T->GetParent();Parent;Parent=Parent->GetParent()) if(auto* B=Cast<UButton>(Parent)) { B->OnClicked.Broadcast(); return true; } return false;
}
class FRoomMenus:public IAutomationLatentCommand {
 FAutomationTestBase* T; int Mode=0,Phase=0; double Began=FPlatformTime::Seconds(),At=0;
public:
 FRoomMenus(FAutomationTestBase* Test):T(Test) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>60) { T->AddError(TEXT("Room menu test timeout")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false; auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); if(!G||!W->GetName().Contains(TEXT("L_MainMenu"))) return false;
  if(Phase==0) { G->OpenModeEntry(ECultPlayMode(Mode)); Phase=1; return false; }
  TArray<UUserWidget*> UI; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,UI,UCultRoomMenuWidget::StaticClass(),true);
  if(Phase==1) { if(UI.Num()!=1) return false; T->TestTrue(TEXT("Enter Match starts real search"),RoomClick(UI[0],TEXT("ENTER MATCH"))); At=FPlatformTime::Seconds(); Phase=2; return false; }
  if(Phase==2) { if(FPlatformTime::Seconds()-At<3.5) return false; T->TestTrue(TEXT("No-room search returns actionable feedback"),G->RoomAction==ECultRoomAction::None&&G->RoomStatus.Contains(TEXT("No open room"))); T->TestTrue(TEXT("Private Room callback"),UI.Num()==1&&RoomClick(UI[0],TEXT("PRIVATE ROOM"))); T->TestTrue(TEXT("Private room has invitation and hosting state"),G->RoomPrivate&&G->RoomHosting&&G->RoomKey.Len()==16); G->OpenModeEntry(ECultPlayMode(Mode)); Phase=3; return false; }
  if(Phase==3) { if(UI.Num()!=1) return false; T->TestTrue(TEXT("Host callback"),RoomClick(UI[0],TEXT("ENTER / HOST"))); T->TestTrue(TEXT("Public hosting is discoverable"),G->RoomHosting&&!G->RoomPrivate&&G->RoomKey.IsEmpty()); G->OpenModeEntry(ECultPlayMode(Mode)); Phase=4; return false; }
  if(Phase==4) { if(UI.Num()!=1) return false; T->TestTrue(TEXT("Code callback"),RoomClick(UI[0],TEXT("ENTER ROOM CODE"))); T->TestTrue(TEXT("Invalid invitation stays in menu with feedback"),G->RoomAction==ECultRoomAction::None&&!G->RoomStatus.IsEmpty()); RoomClick(UI[0],TEXT("BACK")); if(++Mode==3) return true; Phase=0; }
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultRoomMenuTest,"CultUlhu.Network.RoomMenus",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultRoomMenuTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FRoomMenus(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
