#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "CultHUDSpace.h"
#include "CultPlayHUD.h"
#include "beliefs/BeliefSystem.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPopulation.h"
#include "CultWorldDirector.h"
#include "CultMatchState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

struct FCultHUDCoordinateTestAccess {
 static TOptional<FBox2D> Command(ACultPlayHUD* H,int Id) { for(int I=0;I<H->Actions_.Num()&&I<H->Buttons_.Num();++I) if(H->Actions_[I]==Id) return H->Buttons_[I]; return {}; }
 static TOptional<FBox2D> Faith(ACultPlayHUD* H,const FString& Key) { for(int I=0;I<H->FaithActions_.Num()&&I<H->FaithButtons_.Num();++I) if(H->FaithActions_[I]==Key) return H->FaithButtons_[I]; return {}; }
 static const TArray<int32>& Gauges(ACultPlayHUD* H) { return H->FaithGaugeBeliefs_; }
 static bool Outcomes(ACultPlayHUD* H) { return H->bFaithOutcomes_; }
 static int Belief(ACultPlayHUD* H) { return H->SelectedBelief_; }
 static bool Selecting(ACultPlayHUD* H) { return H->bSelecting_; }
 static bool Commands(ACultPlayHUD* H) { return H->bCommands_; }
 static FVector2D Start(ACultPlayHUD* H) { return H->SelectionStart_; }
 static TOptional<FBox2D> Resume(ACultMatchDirector* M) { return M&&M->State_&&M->State_->ResultButtons.Num()?TOptional<FBox2D>(M->State_->ResultButtons[0]):TOptional<FBox2D>(); }
};
namespace {
void CoordinateKey(APlayerController* PC,FKey Key,EInputEvent Event) { PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,Event,Event==IE_Released?0.f:1.f,false,FPlatformTime::Cycles64())); }
}
class FCultHUDCoordinateObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; bool Competitive; int Phase=0,Shape=0,ClickStep=0; double Began=FPlatformTime::Seconds(),At=0,ClickAt=0;
 TWeakObjectPtr<ACameraActor> Camera; TWeakObjectPtr<AStaticMeshActor> Floor; TArray<TWeakObjectPtr<ACultPopulation>> People;
 FVector2D DragMin,DragMax,OldMouse; bool HadMouse=false; FIntRect Rect; FBox2D ClickBox;
 FString CSV=TEXT("mode,shape,view_x,view_y,view_w,view_h,raw_x,raw_y,button_x,button_y\n");
 FString ShapeName() const { return Shape==0?TEXT("pillarbox"):Shape==1?TEXT("letterbox"):TEXT("unconstrained"); }
 void HoldAlt(APlayerController* PC) { if(!PC->IsInputKeyDown(EKeys::LeftAlt)) CoordinateKey(PC,EKeys::LeftAlt,IE_Pressed); }
 bool Mouse(APlayerController* PC,FVector2D Local) {
  FIntRect Current; if(!CultHUDSpace::View(PC,Current)) return false;
  const FVector2D Raw=Local+FVector2D(Current.Min); PC->SetMouseLocation(FMath::RoundToInt(Raw.X),FMath::RoundToInt(Raw.Y));
  float X=0,Y=0; return PC->GetMousePosition(X,Y)&&FVector2D::Distance(Raw,FVector2D(X,Y))<2;
 }
 bool Click(APlayerController* PC,const TOptional<FBox2D>& Box,bool Alt=true) {
  const double Now=FPlatformTime::Seconds();
  if(ClickStep==0) { if(!Box.IsSet()) return false; ClickBox=Box.GetValue(); ClickAt=Now; ClickStep=1; }
  if(ClickStep==1) {
   if(Alt) HoldAlt(PC); if(!Mouse(PC,ClickBox.GetCenter())||Now-ClickAt<.12) return false;
   float X=0,Y=0; PC->GetMousePosition(X,Y);
   CSV+=FString::Printf(TEXT("%s,%s,%d,%d,%d,%d,%.1f,%.1f,%.1f,%.1f\n"),Competitive?TEXT("match"):TEXT("freeroam"),*ShapeName(),Rect.Min.X,Rect.Min.Y,Rect.Width(),Rect.Height(),X,Y,ClickBox.GetCenter().X,ClickBox.GetCenter().Y);
   CoordinateKey(PC,EKeys::LeftMouseButton,IE_Pressed); ClickStep=2; ClickAt=Now; return false;
  }
  if(Now-ClickAt<.12) return false;
  CoordinateKey(PC,EKeys::LeftMouseButton,IE_Released); ClickStep=0; return true;
 }
public:
 FCultHUDCoordinateObservation(FAutomationTestBase* T,bool Match):Test(T),Competitive(Match) {}
 bool Update() override {
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) return false;
  const double Now=FPlatformTime::Seconds();
  if(Now-Began>150) { Test->AddError(FString::Printf(TEXT("HUD coordinate input timed out in phase %d, shape %d, click %d"),Phase,Shape,ClickStep)); UGameplayStatics::SetGamePaused(W,false); return true; }
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(Phase>1) { Test->AddError(TEXT("A HUD click unexpectedly left gameplay for the main menu")); return true; }
   if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("mi_go"); G->OpenCharacterSelect(Competitive?ECultPlayMode::Domination:ECultPlayMode::FreeRoam); if(Competitive) G->OpenBeliefSelect(); else { G->SetInitialBeliefs({5,6,11}); G->StartPlay(); } Phase=1; } return false;
  }
  auto* PC=W->GetFirstPlayerController(); auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr; auto* H=PC?Cast<ACultPlayHUD>(PC->GetHUD()):nullptr; if(!P||!H||W->GetTimeSeconds()<3) return false;
  auto* D=ACultWorldDirector::Find(W); auto* M=ACultMatchDirector::Find(W);
  if(Phase==1) {
   float MouseX=0,MouseY=0; HadMouse=PC->GetMousePosition(MouseX,MouseY); OldMouse=FVector2D(MouseX,MouseY);
   Camera=W->SpawnActor<ACameraActor>(FVector(0,-1800,45900),FRotator::ZeroRotator); Camera->SetActorRotation((FVector(0,0,45100)-Camera->GetActorLocation()).Rotation()); Camera->GetCameraComponent()->SetFieldOfView(60); PC->SetViewTarget(Camera.Get());
   if(!Competitive) {
    Floor=W->SpawnActor<AStaticMeshActor>(FVector(0,0,45000),FRotator::ZeroRotator); auto* Mesh=Floor->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); Floor->SetActorScale3D(FVector(40,30,.2));
    for(float X:{-300.f,-100.f,600.f}) { auto* Unit=W->SpawnActor<ACultPopulation>(FVector(X,0,45100),FRotator::ZeroRotator); Unit->RecruitTo(0); Unit->Order(TEXT("Pray"),Unit->GetActorLocation(),nullptr,1000,true); Unit->GetCharacterMovement()->DisableMovement(); People.Add(Unit); }
   }
   Phase=10;
  }
  if(Phase==10) {
   if(Shape>=3) {
    CoordinateKey(PC,EKeys::LeftAlt,IE_Released); CoordinateKey(PC,EKeys::LeftMouseButton,IE_Released); P->AltReleased();
    if(H->FaithVisible()) H->ToggleFaith(); if(M&&M->IsPaused()) M->TogglePause();
    for(auto& Unit:People) if(Unit.IsValid()) Unit->Destroy(); if(D&&!Competitive) D->SelectFollowers({});
    PC->SetViewTarget(P); if(Camera.IsValid()) Camera->Destroy(); if(Floor.IsValid()) Floor->Destroy(); if(HadMouse) PC->SetMouseLocation(FMath::RoundToInt(OldMouse.X),FMath::RoundToInt(OldMouse.Y));
    const auto Dir=FPaths::ProjectSavedDir()/TEXT("Reports"); IFileManager::Get().MakeDirectory(*Dir,true);
    Test->TestTrue(TEXT("Actual cursor/view/button coordinates are recorded"),FFileHelper::SaveStringToFile(CSV,*(Dir/(Competitive?TEXT("MatchHUDCoordinates.csv"):TEXT("ArmyHUDCoordinates.csv"))))); return true;
   }
   int Width=0,Height=0; PC->GetViewportSize(Width,Height); const float Aspect=float(Width)/FMath::Max(1,Height);
   Camera->GetCameraComponent()->bConstrainAspectRatio=Shape<2; Camera->GetCameraComponent()->SetAspectRatio(Aspect*(Shape==0?.65f:1.2f));
   CoordinateKey(PC,EKeys::LeftAlt,IE_Released); P->AltReleased(); if(H->FaithVisible()) H->ToggleFaith(); if(D&&!Competitive) { D->SelectFollowers({}); if(D->BattlePrepared()) D->Execute(ACultWorldDirector::PrepareBattle,nullptr,P->FeetLocation()); }
   At=Now; Phase=11; return false;
  }
  if(Phase==11) {
   if(Now-At<.3||!CultHUDSpace::View(PC,Rect)) return false;
   Test->AddInfo(FString::Printf(TEXT("%s %s: view origin %d,%d size %d,%d"),Competitive?TEXT("match"):TEXT("freeroam"),*ShapeName(),Rect.Min.X,Rect.Min.Y,Rect.Width(),Rect.Height()));
   Test->TestTrue(TEXT("Fixture actually has the intended camera bars"),Shape==0?Rect.Min.X>20:Shape==1?Rect.Min.Y>10:Rect.Min==FIntPoint::ZeroValue);
   if(Competitive) { CoordinateKey(PC,EKeys::Escape,IE_Pressed); At=Now; Phase=30; return false; }
   if(Shape<2) {
    const FVector2D Bar=Shape==0?FVector2D(-Rect.Min.X*.5f,Rect.Height()*.5f):FVector2D(Rect.Width()*.5f,-Rect.Min.Y*.5f);
    HoldAlt(PC); if(!Mouse(PC,Bar)) return false; CoordinateKey(PC,EKeys::LeftMouseButton,IE_Pressed); At=Now; Phase=21; return false;
   }
   Phase=23;
  }
  if(Phase==21) {
   if(Now-At<.15) return false;
   Test->TestFalse(TEXT("Clicking a camera bar cannot begin army selection"),FCultHUDCoordinateTestAccess::Selecting(H));
   CoordinateKey(PC,EKeys::LeftMouseButton,IE_Released); At=Now; Phase=22; return false;
  }
  if(Phase==22) { if(Now-At<.15) return false; Test->TestFalse(TEXT("Releasing a camera-bar click cannot open world orders"),FCultHUDCoordinateTestAccess::Commands(H)); Phase=23; }
  if(Phase==23) {
   FVector2D A,B; PC->ProjectWorldLocationToScreen(People[0]->GetActorLocation(),A,true); PC->ProjectWorldLocationToScreen(People[1]->GetActorLocation(),B,true);
   DragMin=FVector2D(FMath::Min(A.X,B.X)-16,FMath::Min(A.Y,B.Y)-16); DragMax=FVector2D(FMath::Max(A.X,B.X)+16,FMath::Max(A.Y,B.Y)+16);
   HoldAlt(PC); if(!Mouse(PC,DragMin)) return false; CoordinateKey(PC,EKeys::LeftMouseButton,IE_Pressed); At=Now; Phase=12; return false;
  }
  if(Phase==12) {
   HoldAlt(PC); Mouse(PC,DragMin); if(Now-At<.15) return false;
   Test->TestTrue(TEXT("Actual ALT+LMB begins drag selection"),FCultHUDCoordinateTestAccess::Selecting(H));
   Test->TestTrue(TEXT("Each camera shape actually shows the command cursor and overlay"),PC->bShowMouseCursor&&PC->IsInputKeyDown(EKeys::LeftAlt));
   Test->TestTrue(TEXT("Visible drag starts at the actual local pointer"),FVector2D::Distance(FCultHUDCoordinateTestAccess::Start(H),DragMin)<2);
   Mouse(PC,DragMax); At=Now; Phase=13; return false;
  }
  if(Phase==13) {
   HoldAlt(PC); Mouse(PC,DragMax); if(Now-At<.15) return false;
   FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(TEXT("ArmyDrag_")+ShapeName()+TEXT(".png"))),true,false); Phase=14; return false;
  }
  if(Phase==14) { HoldAlt(PC); Mouse(PC,DragMax); if(FScreenshotRequest::IsScreenshotRequested()) return false; CoordinateKey(PC,EKeys::LeftMouseButton,IE_Released); At=Now; Phase=15; return false; }
  if(Phase==15) {
   HoldAlt(PC); if(Now-At<.15) return false;
   Test->TestEqual(TEXT("Dragging selects exactly the two visible followers"),D->SelectedCount(),2);
   Test->TestTrue(TEXT("Drag leaves the third follower unselected"),D->IsSelected(People[0].Get())&&D->IsSelected(People[1].Get())&&!D->IsSelected(People[2].Get()));
   for(int Id=ACultWorldDirector::RestFollowers;Id<=ACultWorldDirector::PunishCultist;++Id) {
    const auto Box=FCultHUDCoordinateTestAccess::Command(H,Id); Test->TestTrue(TEXT("Every faith action has a visible ALT hit region"),Box.IsSet()&&Box->Min.X>=0&&Box->Max.X<=Rect.Width()&&Box->Min.Y>=0&&Box->Max.Y<=Rect.Height());
   }
   FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(TEXT("FaithOrders_")+ShapeName()+TEXT(".png"))),true,false);
   Phase=16;
  }
  if(Phase==16) {
   if(!Click(PC,FCultHUDCoordinateTestAccess::Command(H,ACultWorldDirector::PrepareBattle))) return false;
   Test->TestTrue(TEXT("Clicking the rendered Prepare for battle button prepares the selected group"),D->BattlePrepared()&&D->PreparedCount()==2);
   CoordinateKey(PC,EKeys::B,IE_Pressed); At=Now; Phase=17; return false;
  }
  if(Phase==17) { if(Now-At<.15) return false; CoordinateKey(PC,EKeys::B,IE_Released); Test->TestTrue(TEXT("Real B input opens Free Roam beliefs"),H->FaithVisible()); CoordinateKey(PC,EKeys::LeftAlt,IE_Released); P->AltReleased();
   Test->TestTrue(TEXT("B keeps its own mouse cursor without ALT"),PC->bShowMouseCursor);
   Test->TestEqual(TEXT("Only the three adopted beliefs have rendered gauges"),FCultHUDCoordinateTestAccess::Gauges(H).Num(),3);
   for(int Id:FCultHUDCoordinateTestAccess::Gauges(H)) Test->TestTrue(TEXT("Each gauge belongs to an active belief"),D->Beliefs().isActive(static_cast<cultulhu::Belief>(Id)));
   Test->TestFalse(TEXT("Torture is removed from B"),FCultHUDCoordinateTestAccess::Faith(H,TEXT("Torture")).IsSet());
   FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(TEXT("CompactBeliefs_")+ShapeName()+TEXT(".png"))),true,false);
   Phase=18; }
  if(Phase==18) {
   if(!Click(PC,FCultHUDCoordinateTestAccess::Faith(H,TEXT("outcomes")),false)) return false;
   Test->TestTrue(TEXT("The rendered Outcomes button receives its click"),FCultHUDCoordinateTestAccess::Outcomes(H)); Phase=19;
  }
  if(Phase==19) {
   if(!Click(PC,FCultHUDCoordinateTestAccess::Faith(H,TEXT("select:0")),false)) return false;
   Test->TestTrue(TEXT("The rendered first belief receives its click"),FCultHUDCoordinateTestAccess::Belief(H)==0&&!FCultHUDCoordinateTestAccess::Outcomes(H)); Phase=20;
  }
  if(Phase==20) {
   if(!Click(PC,FCultHUDCoordinateTestAccess::Faith(H,TEXT("adopt:0")),false)) return false;
   Test->TestTrue(TEXT("The real replacement button starts delayed belief adoption"),D->Beliefs().adoptionRemaining(cultulhu::Belief::Torture)>=0&&!D->Beliefs().isActive(cultulhu::Belief::Torture));
   D->Beliefs().restoreActive({cultulhu::Belief::Conversion,cultulhu::Belief::War,cultulhu::Belief::Dreams}); Phase=24;
  }
  if(Phase==24) {
   if(!Click(PC,FCultHUDCoordinateTestAccess::Faith(H,TEXT("close")),false)) return false;
   Test->TestFalse(TEXT("The rendered Close button closes beliefs"),H->FaithVisible()); ++Shape; Phase=10; return false;
  }
  if(Phase==30) {
   if(Now-At<.15) return false; CoordinateKey(PC,EKeys::Escape,IE_Released); Test->TestTrue(TEXT("Real ESC input opens the local match menu"),M&&M->IsPaused()&&!UGameplayStatics::IsGamePaused(W)); Phase=31;
  }
  if(Phase==31) {
   if(!Click(PC,FCultHUDCoordinateTestAccess::Resume(M),false)) return false;
   Test->TestTrue(TEXT("Clicking the rendered Resume button unpauses the actual match"),M&&!M->IsPaused()&&!UGameplayStatics::IsGamePaused(W));
   if(M&&M->IsPaused()) M->TogglePause(); ++Shape; Phase=10; return false;
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultArmyHUDCoordinateTest,"CultUlhu.Menu.ArmyHUDCoordinates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultArmyHUDCoordinateTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FCultHUDCoordinateObservation(this,false)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMatchHUDCoordinateTest,"CultUlhu.Menu.MatchHUDCoordinates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMatchHUDCoordinateTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FCultHUDCoordinateObservation(this,true)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
