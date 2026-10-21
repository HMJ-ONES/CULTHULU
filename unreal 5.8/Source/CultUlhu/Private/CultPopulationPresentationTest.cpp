#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPopulation.h"
#include "CultCorpse.h"
#include "CultPlayHUD.h"
#include "CultWorldDirector.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// Isolated art/inspection fixture, not a screenshot of normal Free Roam density.
class FCultPopulationPresentation : public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0; double Began=FPlatformTime::Seconds(); float At=0;
 TArray<TWeakObjectPtr<ACultPopulation>> People; TArray<TArray<FTransform>> Before; bool Animated[3]={};
 TWeakObjectPtr<ACameraActor> Camera; TWeakObjectPtr<AStaticMeshActor> Floor,Wall; FString Screenshot;
public:
 explicit FCultPopulationPresentation(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>90) { Test->AddError(TEXT("Population presentation timed out")); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("mi_go"); G->OpenCharacterSelect(ECultPlayMode::FreeRoam); G->SetInitialBeliefs({5,6,11}); G->StartPlay(); Phase=1; } return false; }
  auto* PC=W->GetFirstPlayerController(); auto* Player=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr; auto* HUD=PC?Cast<ACultPlayHUD>(PC->GetHUD()):nullptr; if(!Player||!HUD||W->GetTimeSeconds()<3) return false;
  const float Now=W->GetTimeSeconds();
  if(Phase==1) {
   Floor=W->SpawnActor<AStaticMeshActor>(FVector(0,0,45000),FRotator::ZeroRotator);
   Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
   Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); Floor->SetActorScale3D(FVector(24,20,.2));
   Test->TestTrue(TEXT("Presentation platform has a real supporting mesh"),Floor->GetStaticMeshComponent()->GetStaticMesh()!=nullptr);
   Camera=W->SpawnActor<ACameraActor>(FVector(0,-1200,45600),FRotator::ZeroRotator); Camera->SetActorRotation((FVector(0,0,45150)-Camera->GetActorLocation()).Rotation()); Camera->GetCameraComponent()->SetFieldOfView(60); PC->SetViewTarget(Camera.Get());
   for(int I=0;I<3;++I) {
    auto* P=W->SpawnActor<ACultPopulation>(FVector((I-1)*500,0,45100),FRotator(0,-90,0));
    if(!P) { Test->AddError(TEXT("Presentation actor failed to spawn")); return true; }
    P->RecruitTo(0); if(I==2) P->SetSorcerer(true); P->GetCharacterMovement()->DisableMovement(); P->SetActorTickEnabled(false);
    P->Order(I==0?TEXT("Pray"):I==1?TEXT("Build"):TEXT("Convert"),P->GetActorLocation(),nullptr,1000,true); P->Tick(.01f);
    Test->TestTrue(TEXT("Each activity retains the imported articulated body"),P->HasAuthoredHumanModel()&&P->HumanPartCount()>=6);
    auto* Pose=P->FindComponentByClass<UPoseableMeshComponent>(); Before.Add(Pose?Pose->BoneSpaceTransforms:TArray<FTransform>()); People.Add(P);
   }
   Player->AltReleased(); At=Now; Phase=2; return false;
  }
  if(Phase==2) {
   for(int I=0;I<People.Num();++I) if(People[I].IsValid()) {
    auto* P=People[I].Get(); P->Tick(W->GetDeltaSeconds());
    Test->TestEqual(TEXT("Releasing ALT removes every inspection card"),HUD->InspectionCards().Num(),0);
    Test->TestNull(TEXT("NPCs no longer allocate blurry world-space text components"),P->FindComponentByClass<UTextRenderComponent>());
    if(auto* Pose=P->FindComponentByClass<UPoseableMeshComponent>()) for(int B=0;B<FMath::Min(Before[I].Num(),Pose->BoneSpaceTransforms.Num());++B) Animated[I]|=!Before[I][B].Equals(Pose->BoneSpaceTransforms[B],.001f);
   }
   if(Now-At<1.2f) return false;
   for(int I=0;I<3;++I) Test->TestTrue(TEXT("Visible authored activity continues animating with inspection closed"),Animated[I]);
   People[1]->SetWorkProgress(.48f,true); People[1]->ShowContribution(TEXT("CONSTRUCTION + BELIEF"));
   ACultWorldDirector::Find(W)->SelectFollowers({People[1].Get()}); Player->AltPressed(); At=Now; Phase=3; return false;
  }
  if(Phase==3) {
   if(Now-At<.2f) return false;
   Test->TestTrue(TEXT("Inspection image uses its close camera"),PC->GetViewTarget()==Camera.Get()&&FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(),Camera->GetActorLocation())<10);
   for(int I=0;I<3;++I) {
    auto* P=People[I].Get(); P->Tick(.01f);
    const auto* Card=HUD->InspectionCards().FindByPredicate([P](const FCultInspectionCard& C){return C.Person==P;});
    Test->TestTrue(TEXT("ALT immediately renders the current activity and health"),Card&&Card->Activity==P->InspectionActivity()&&Card->Health==100&&Card->MaxHealth==100);
    if(I==1) Test->TestTrue(TEXT("Rendered inspection text preserves the full progress and belief receipt"),Card&&Card->Selected&&Card->Activity.Contains(TEXT("48%"))&&Card->Contribution==TEXT("CONSTRUCTION + BELIEF"));
    TArray<UPoseableMeshComponent*> Parts; P->GetComponents(Parts); UPoseableMeshComponent* Leader=nullptr;
    for(auto* Part:Parts) if(!Part->LeaderPoseComponent.IsValid()) Leader=Part;
    Test->TestNotNull(TEXT("Authored body retains a pose leader"),Leader);
    for(auto* Part:Parts) Test->TestTrue(TEXT("Body and every limb remain visible with one shared pose"),Part->IsVisible()&&(Part==Leader||Part->LeaderPoseComponent.Get()==Leader));
   }
   Screenshot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/PopulationInspection.png")); FScreenshotRequest::RequestScreenshot(Screenshot,true,false); Phase=4; At=Now; return false;
  }
  if(Phase==4) {
   if(Now-At<.3f||FScreenshotRequest::IsScreenshotRequested()) return false;
   Test->TestTrue(TEXT("Rendered NPC inspection fixture is saved"),IFileManager::Get().FileSize(*Screenshot)>0);
   auto* Source=People[2].Get(); auto* Corpse=ACultCorpse::Leave(Source); Test->TestNotNull(TEXT("Authored animated NPC leaves a body"),Corpse);
   if(Corpse) { TArray<UPoseableMeshComponent*> Parts; Corpse->GetComponents(Parts); Test->TestEqual(TEXT("Corpse preserves all six authored body parts"),Parts.Num(),Source->HumanPartCount()); for(auto* Part:Parts) Test->TestTrue(TEXT("Corpse pose is persistent and nonblocking"),Part->IsVisible()&&!Part->IsComponentTickEnabled()&&Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision); Corpse->Destroy(); }
   // Add a dense group, an actor behind the camera and a wall hiding one unit.
   for(int I=0;I<12;++I) { auto* P=W->SpawnActor<ACultPopulation>(FVector((I%3-1)*30,40+(I/3)*25,45100),FRotator::ZeroRotator); P->RecruitTo(0); P->GetCharacterMovement()->DisableMovement(); P->SetActorTickEnabled(false); P->Tick(.01f); People.Add(P); }
   const FVector Back=Camera->GetActorLocation()-Camera->GetActorForwardVector()*1000;
   auto* Behind=W->SpawnActor<ACultPopulation>(Back,FRotator::ZeroRotator); Behind->GetCharacterMovement()->DisableMovement(); Behind->SetActorTickEnabled(false); Behind->Tick(.01f); People.Add(Behind);
   const FVector Mid=(Camera->GetActorLocation()+People[0]->GetActorLocation()+FVector(0,0,113))*.5f;
   Wall=W->SpawnActor<AStaticMeshActor>(Mid,FRotator::ZeroRotator); Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); Wall->SetActorScale3D(FVector(2.5f,.8f,6));
   At=Now; Phase=5; return false;
  }
  if(Phase==5) {
   if(Now-At<.4f) return false;
   const auto& Cards=HUD->InspectionCards();
   Test->TestTrue(TEXT("Dense crowds retain neighboring cards beside the selected worker"),Cards.Num()>=3&&Cards.Num()<=8);
   Test->TestNotNull(TEXT("Selected worker keeps priority in a crowded group"),Cards.FindByPredicate([this](const FCultInspectionCard& C){return C.Person==People[1];}));
   Test->TestNull(TEXT("Inspection never reveals someone hidden behind scenery"),Cards.FindByPredicate([this](const FCultInspectionCard& C){return C.Person==People[0];}));
   Test->TestNull(TEXT("Actors behind the camera have no inspection card"),Cards.FindByPredicate([this](const FCultInspectionCard& C){return C.Person==People.Last();}));
   for(const auto& Card:Cards) Test->TestTrue(TEXT("Crowd labels stay above their body's head"),Card.Bounds.Max.Y<=Card.Anchor.Y);
   for(int I=0;I<Cards.Num();++I) for(int J=I+1;J<Cards.Num();++J) Test->TestFalse(TEXT("Dense inspection cards never overlap"),Cards[I].Bounds.Intersect(Cards[J].Bounds));
   Screenshot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/PopulationInspectionCrowd.png")); FScreenshotRequest::RequestScreenshot(Screenshot,true,false); Phase=6; At=Now; return false;
  }
  if(Phase==6) {
   if(Now-At<.3f||FScreenshotRequest::IsScreenshotRequested()) return false;
   Test->TestTrue(TEXT("Crowded occlusion inspection is rendered"),IFileManager::Get().FileSize(*Screenshot)>0);
   Player->AltReleased(); At=Now; Phase=7; return false;
  }
  if(Phase==7) {
   if(Now-At<.1f) return false;
   Test->TestEqual(TEXT("ALT release immediately clears rendered cards"),HUD->InspectionCards().Num(),0);
   for(auto& P:People) if(P.IsValid()) P->Destroy(); if(Wall.IsValid()) Wall->Destroy(); ACultWorldDirector::Find(W)->SelectFollowers({});
   PC->SetViewTarget(Player); if(Camera.IsValid()) Camera->Destroy(); if(Floor.IsValid()) Floor->Destroy(); return true;
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultPopulationPresentationTest,"CultUlhu.Menu.PopulationPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultPopulationPresentationTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FCultPopulationPresentation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
