#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuPlayerController.h"
#include "CultCharacterModel.h"
#include "CultMatchDirector.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

class FCultHeadCameraAimCheck:public IAutomationLatentCommand {
 FAutomationTestBase* Test; ECultPlayMode Mode; int Phase=0; float At=0; double Began=FPlatformTime::Seconds();
public:
 FCultHeadCameraAimCheck(FAutomationTestBase* T,ECultPlayMode M):Test(T),Mode(M) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>100) {Test->AddError(TEXT("Bokrug head-camera test timed out")); return true;}
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) {auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance());G->SelectedMode=Mode;G->SelectedCharacterId=TEXT("bokrug");G->SetInitialBeliefs({1,2,3});G->FreeRoamSaveSlot=TEXT("Automation_HeadCamera");G->StartPlay();Phase=1;} return false;
  }
  auto* PC=Cast<ACultUlhuPlayerController>(W->GetFirstPlayerController()); auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr;
  if(!P||W->GetTimeSeconds()<1) return false;
  auto* Camera=P->FindComponentByClass<UCameraComponent>(); if(!Camera) return false;
  if(Phase==1) {
   if(auto* M=ACultMatchDirector::Find(W)) M->SetActorTickEnabled(false);
   P->RelocateMatch(FVector(200000,0,60000)); P->SetActorTickEnabled(false);P->GetCharacterMovement()->DisableMovement();
   if(P->IsThirdPerson()) P->ToggleCamera(); PC->SetControlRotation(FRotator::ZeroRotator); At=W->GetTimeSeconds();Phase=2;return false;
  }
  if(W->GetTimeSeconds()-At<.5f) return false;
  if(Phase==2) {
   PC->PlayerCameraManager->UpdateCamera(0);
   TArray<AActor*> Attached; P->GetAttachedActors(Attached); ACultCharacterModel* Model=nullptr;
   for(auto* Actor:Attached) if(auto* M=Cast<ACultCharacterModel>(Actor)) Model=M;
   Test->TestNotNull(TEXT("Bokrug has the actual imported model"),Model); if(!Model) return true;
   FVector Head; Test->TestTrue(TEXT("Actual head bones define the eye position"),Model->FirstPersonEye(Head));
   Test->TestTrue(TEXT("First-person camera follows the creature's head"),Camera->GetComponentLocation().Equals(Head,8.f));
   TSet<FPrimitiveComponentId> Hidden; PC->UpdateHiddenComponents(Camera->GetComponentLocation(),Hidden);
   for(UMeshComponent* Part:Model->Parts()) Test->TestTrue(TEXT("Bokrug's body cannot obstruct his first-person view"),Hidden.Contains(Part->GetPrimitiveSceneId()));
   Test->TestFalse(TEXT("View-only hiding preserves other players' model visibility"),Model->IsHidden());
   FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/BokrugHeadCamera_%d.png"),int(Mode))),true,false);
   At=W->GetTimeSeconds();Phase=3;return false;
  }
  P->ToggleCamera();
  Camera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
  const FVector Origin=P->GetActorLocation(),Offset(0,900,500);
  Camera->SetWorldLocationAndRotation(Origin+FVector(-4000,900,500),FRotator::ZeroRotator);PC->PlayerCameraManager->UpdateCamera(0);
  auto Box=[&](FVector Point,FVector Size) {auto* A=W->SpawnActor<AStaticMeshActor>(Point,FRotator::ZeroRotator);auto* C=A->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));C->SetWorldScale3D(Size/100);C->SetCollisionProfileName(TEXT("BlockAll"));return A;};
  auto* Rear=Box(Origin+Offset+FVector(-2000,0,0),FVector(100,3000,4000));
  auto* Target=Box(Origin+Offset+FVector(5000,0,0),FVector(1000));
  auto Aim=P->CameraAim();
  Test->TestEqual(TEXT("Camera-side wall cannot pull third-person aim behind the character"),Aim.GetActor(),static_cast<AActor*>(Target));
  Test->TestTrue(TEXT("Combat aiming starts forward of the character"),FVector::DotProduct(Aim.TraceStart-Origin,FVector::ForwardVector)>0);
  auto* Front=Box(Origin+Offset+FVector(P->GetSimpleCollisionRadius()+200,0,0),FVector(100,3000,4000));
  Aim=P->CameraAim();Test->TestEqual(TEXT("A real wall in front still blocks combat aiming"),Aim.GetActor(),static_cast<AActor*>(Front));
  Test->TestTrue(TEXT("Near-wall shots never target behind the character"),FVector::DotProduct(Aim.ImpactPoint-Origin,FVector::ForwardVector)>0);
  Rear->Destroy();Target->Destroy();Front->Destroy();return true;
 }
};
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCultHeadCameraAimTest,"CultUlhu.Camera.HeadAndForwardAim",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FCultHeadCameraAimTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const {Names={TEXT("FreeRoam"),TEXT("Domination"),TEXT("DOTGOO")};Commands={TEXT("0"),TEXT("1"),TEXT("2")};}
bool FCultHeadCameraAimTest::RunTest(const FString& Parameter) {ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE());ADD_LATENT_AUTOMATION_COMMAND(FCultHeadCameraAimCheck(this,ECultPlayMode(FCString::Atoi(*Parameter))));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
