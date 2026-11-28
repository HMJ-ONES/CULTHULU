#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CultFirstPersonView.h"
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultCharacterModel.h"
#include "CultUlhuPlayerController.h"
#include "CultMatchDirector.h"
#include "CultPrimaryProfile.h"
#include "Camera/CameraComponent.h"
#include "ProceduralMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultBakeFirstPerson,"CultUlhu.Art.BakeFirstPerson",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultBakeFirstPerson::RunTest(const FString&) {
 for(const FString Id:{TEXT("cthulhu_avatar"),TEXT("dagon"),TEXT("yig"),TEXT("mi_go"),TEXT("shoggoths"),TEXT("nyarlathotep"),TEXT("rhan_tegoth")}) TestTrue(TEXT("Baked original textured limb geometry for ")+Id,UCultFirstPersonView::Bake(Id));
 return true;
}
class FCultFirstPersonViewCheck:public IAutomationLatentCommand {
 FAutomationTestBase* Test;FString Id;int Phase=0;float At=0;double Began=FPlatformTime::Seconds();
public:
 FCultFirstPersonViewCheck(FAutomationTestBase* T,FString Character):Test(T),Id(Character) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>100) {Test->AddError(TEXT("First-person view check timed out"));return true;}
  auto* W=GEditor->PlayWorld.Get();if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) {auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance());G->SelectedMode=ECultPlayMode::Domination;G->SelectedCharacterId=Id;G->StartPlay();Phase=1;}return false;
  }
  auto* PC=Cast<ACultUlhuPlayerController>(W->GetFirstPlayerController());auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr;if(!P||W->GetTimeSeconds()<1) return false;
  auto* V=P->FindComponentByClass<UCultFirstPersonView>();
  if(Phase==1) {if(P->IsThirdPerson()) P->ToggleCamera();At=W->GetTimeSeconds();Phase=2;return false;}
  if(W->GetTimeSeconds()-At<(Phase==2?.7f:Phase==3?.22f:.15f)) return false;
  if(Phase==2) if(auto* Match=ACultMatchDirector::Find(W)) if(Match->Warmup()>0) return false;
  if(Phase==2) {
   Test->TestTrue(TEXT("Actual local anatomy is visible at rest: ")+Id,V&&V->HasVisibleAnatomy());
   TArray<UProceduralMeshComponent*> Meshes;P->GetComponents(Meshes);int Vertices=0,Framed=0;
   TSet<FPrimitiveComponentId> Hidden;auto* C=P->FindComponentByClass<UCameraComponent>();PC->UpdateHiddenComponents(C->GetComponentLocation(),Hidden);
   for(auto* Mesh:Meshes) if(Mesh->ComponentHasTag(TEXT("CultFirstPersonAnatomy"))) {
    Test->TestFalse(TEXT("Local arms are not included in body occlusion"),Hidden.Contains(Mesh->GetPrimitiveSceneId()));
    Test->TestEqual(TEXT("View arms never affect collision"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    if(auto* S=Mesh->GetProcMeshSection(0)) {
     Vertices+=S->ProcVertexBuffer.Num();
     for(const auto& Vertex:S->ProcVertexBuffer) {const FVector Q(Vertex.Position);if(Q.X>10&&FMath::Abs(Q.Y)<Q.X*.92f&&FMath::Abs(Q.Z)<Q.X*.52f) ++Framed;}
    }
   }
   Test->TestTrue(TEXT("Viewmodel has geometry within the local vertex budget"),Vertices>50&&Vertices<=24000);
   Test->TestTrue(TEXT("Idle anatomy is actually framed in front of the camera"),Framed>50&&Framed>Vertices*.15f);
   FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/FirstPersonRefresh/")+Id+TEXT("_idle.png"),true,false);
   P->RoutePlayerInput(ECultPlayerInput::Primary,true);
   Test->TestEqual(TEXT("Real primary input starts the creature's actual attack playback"),P->AttackPlayback().Move,CultPrimary::Find(Id).Ranged()?CultAttackMotion::EMove::Primary:CultAttackMotion::EMove::Melee);
   At=W->GetTimeSeconds();Phase=3;return false;
  }
  if(Phase==3) {FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/FirstPersonRefresh/")+Id+TEXT("_attack.png"),true,false);P->RoutePlayerInput(ECultPlayerInput::Primary,false);At=W->GetTimeSeconds();Phase=4;return false;}
  if(Phase==4) {P->ToggleCamera();At=W->GetTimeSeconds();Phase=5;return false;}
  Test->TestFalse(TEXT("Third person disables camera anatomy"),V&&V->HasVisibleAnatomy());return true;
 }
};
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCultFirstPersonViews,"CultUlhu.Camera.FirstPersonAnatomy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FCultFirstPersonViews::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const {Names={TEXT("dagon"),TEXT("cthulhu_avatar"),TEXT("yig"),TEXT("mi_go"),TEXT("shoggoths"),TEXT("nyarlathotep"),TEXT("rhan_tegoth"),TEXT("bokrug"),TEXT("mother_hydra"),TEXT("shub_niggurath"),TEXT("nug_and_yeb")};Commands=Names;}
bool FCultFirstPersonViews::RunTest(const FString& Id) {ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE());ADD_LATENT_AUTOMATION_COMMAND(FCultFirstPersonViewCheck(this,Id));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
