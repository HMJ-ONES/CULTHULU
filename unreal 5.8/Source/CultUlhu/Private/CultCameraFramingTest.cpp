#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

class FCultCameraFramingObservation : public IAutomationLatentCommand {
 FAutomationTestBase* Test; ECultPlayMode Mode; FString Character; int32 Phase=0;
 double Began=FPlatformTime::Seconds(); float At=0,Scale=0,Half=0,BeforeDistance=0;
 TWeakObjectPtr<AStaticMeshActor> Wall;
public:
 FCultCameraFramingObservation(FAutomationTestBase* In,ECultPlayMode Choice,FString Id):Test(In),Mode(Choice),Character(MoveTemp(Id)) {}
 ~FCultCameraFramingObservation() { if(Wall.IsValid()) Wall->Destroy(); }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>100) { Test->AddError(TEXT("Native camera framing check timed out")); return true; }
  auto* World=GEditor->PlayWorld.Get(); if(!World||World->bIsTearingDown) return false;
  if(World->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) { auto* Game=Cast<UCultUlhuGameInstance>(World->GetGameInstance()); Game->SelectedCharacterId=Character; Game->OpenCharacterSelect(Mode); Game->OpenBeliefSelect(); Phase=1; }
   return false;
  }
  if(!Phase||World->GetTimeSeconds()<.7f) return false;
  auto* Player=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(World,0)); auto* Match=ACultMatchDirector::Find(World);
  if(!Player||!Match) return false;
  auto* Arm=Player->FindComponentByClass<USpringArmComponent>(); auto* Camera=Player->FindComponentByClass<UCameraComponent>();
  if(!Arm||!Camera) { Test->AddError(TEXT("Player has no native camera rig")); return true; }
  if(Phase==1) {
   Scale=Player->GetActorScale3D().Z; Half=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
   const auto Aim=Player->CameraAim(15000); auto* Blocker=Cast<ACultUlhuPlayerPawn>(Aim.GetActor());
   Test->TestFalse(TEXT("Initial crosshair has no allied vessel directly blocking the battle view"),Blocker&&Blocker!=Player&&Blocker->GetFaction()==Player->GetFaction());
   const float BodyPlane=FVector::DotProduct(Player->GetActorLocation()-Camera->GetComponentLocation(),Camera->GetForwardVector());
   Test->TestTrue(TEXT("Initial view has no solid obstruction behind the player's body plane"),!Aim.bBlockingHit||Aim.Distance>=BodyPlane-Player->GetSimpleCollisionRadius());
   Test->AddInfo(FString::Printf(TEXT("Camera %s mode=%d player=%s eye=%s arm=%.1f fixed=%d aim=%s"),*Character,int32(Mode),*Player->GetActorLocation().ToCompactString(),*Camera->GetComponentLocation().ToCompactString(),Arm->TargetArmLength,int32(Arm->IsCollisionFixApplied()),*GetNameSafe(Aim.GetActor())));
   Test->TestTrue(TEXT("Match starts in the requested third-person view"),Player->IsThirdPerson());
   Player->ToggleCamera(); At=World->GetTimeSeconds(); Phase=2; return false;
  }
  if(World->GetTimeSeconds()-At<.2f) return false;
  if(Phase==2) {
   Test->TestFalse(TEXT("V toggle enters first person"),Player->IsThirdPerson());
   Test->TestEqual(TEXT("First person collapses the actual camera boom"),Arm->TargetArmLength,0.f);
   Player->ToggleCamera(); At=World->GetTimeSeconds(); Phase=3; return false;
  }
  if(Phase==3) {
   Test->TestTrue(TEXT("V returns to third person"),Player->IsThirdPerson());
   Test->TestTrue(TEXT("Camera changes preserve entity scale and capsule height"),FMath::IsNearlyEqual(Scale,float(Player->GetActorScale3D().Z),.001f)&&FMath::IsNearlyEqual(Half,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),.01f));
   BeforeDistance=FVector::Distance(Arm->GetComponentLocation(),Camera->GetComponentLocation());
   Test->TestTrue(TEXT("Third-person camera has usable boom distance"),BeforeDistance>500);
   Wall=World->SpawnActor<AStaticMeshActor>((Arm->GetComponentLocation()+Camera->GetComponentLocation())*.5,FRotator::ZeroRotator);
   auto* Mesh=Wall->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); Wall->SetActorScale3D(FVector(4,4,50));
   At=World->GetTimeSeconds(); Phase=4; return false;
  }
  const float AfterDistance=FVector::Distance(Arm->GetComponentLocation(),Camera->GetComponentLocation());
  Test->TestTrue(TEXT("Real wall collision still retracts the camera rather than clipping through"),Arm->IsCollisionFixApplied()&&AfterDistance<BeforeDistance-100);
  Test->AddInfo(FString::Printf(TEXT("Camera wall distance %.1f -> %.1f; scale %.3f capsuleHalf %.1f"),BeforeDistance,AfterDistance,Scale,Half));
  return true;
 }
};
#define CULT_CAMERA_TEST(Type,Path,Mode,Character) \
 IMPLEMENT_SIMPLE_AUTOMATION_TEST(Type,Path,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
 bool Type::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultCameraFramingObservation(this,Mode,Character)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
CULT_CAMERA_TEST(FCultDomCameraTest,"CultUlhu.Menu.CameraFraming.DominationMiGo",ECultPlayMode::Domination,TEXT("mi_go"))
CULT_CAMERA_TEST(FCultDomCthulhuCameraTest,"CultUlhu.Menu.CameraFraming.DominationCthulhu",ECultPlayMode::Domination,TEXT("cthulhu_avatar"))
CULT_CAMERA_TEST(FCultMobaCameraTest,"CultUlhu.Menu.CameraFraming.MobaMiGo",ECultPlayMode::Dotgoo,TEXT("mi_go"))
CULT_CAMERA_TEST(FCultMobaCthulhuCameraTest,"CultUlhu.Menu.CameraFraming.MobaCthulhu",ECultPlayMode::Dotgoo,TEXT("cthulhu_avatar"))
#undef CULT_CAMERA_TEST
#endif
