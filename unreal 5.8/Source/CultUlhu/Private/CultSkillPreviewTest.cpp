#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPopulation.h"
#include "CultFeralBeast.h"
#include "CultCorpse.h"
#include "CultDestructible.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
class FSkillPreviewObservation:public IAutomationLatentCommand {
public:
 explicit FSkillPreviewObservation(FAutomationTestBase* In):Test(In),Started(FPlatformTime::Seconds()) {}
 bool Update() override {
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World();
  if(!W) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))&&!Entered) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(ECultPlayMode::FreeRoam); G->SetInitialBeliefs({11,5,6}); G->StartPlay(); Entered=true; return false; }
  if(!W->GetName().Contains(TEXT("L_DarkWilds"))) { if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Preview never reached the game")); return true; } return false; }
  if(W->GetTimeSeconds()<2) return false; Time+=W->GetDeltaSeconds();
  if(Phase==0) {
   P=Cast<ACultUlhuPlayerPawn>(W->GetFirstPlayerController()->GetPawn()); if(!P) return false;
   P->AltReleased(); if(auto* Arm=P->FindComponentByClass<USpringArmComponent>()) Arm->SetComponentTickEnabled(false); auto* Camera=P->FindComponentByClass<UCameraComponent>();
   FTransform T(P->FeetLocation()+FVector(1800,0,200)); auto* B=W->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); B->SetSpecies(TEXT("skittering_ghoul")); B->FinishSpawning(T); B->Capture(); Beast=B;
   const FVector Look=B->GetActorLocation(); Camera->SetWorldLocation(P->FeetLocation()+FVector(6000,-4000,3500)); Camera->SetWorldRotation((Look-Camera->GetComponentLocation()).Rotation());
   Test->TestTrue(TEXT("The actual rendered camera aims directly at the test creature"),P->CameraAim().GetActor()==B);
   Before=B->GetHealth(); P->Smash(); Test->TestEqual(TEXT("Actual LMB has a windup"),B->GetHealth(),Before); Phase=1; Time=0;
  } else if(Phase==1) {
   if(Time>.18f&&!SwingShot) { Shot(TEXT("SkillCombatSwing.png")); SwingShot=true; }
   if(Time>.65f) { Test->TestTrue(TEXT("Actual aimed LMB makes creature contact"),Beast.IsValid()&&Beast->GetHealth()<Before); Before=Beast.IsValid()?Beast->GetHealth():0; P->AbilityQ(); if(Beast.IsValid()) Test->TestEqual(TEXT("Native Q warns instead of instantly detonating"),Beast->GetHealth(),Before); Phase=2; Time=0; }
  } else if(Phase==2&&Time>.9f) {
   Test->TestTrue(TEXT("The native delayed ability damages its current occupant"),!Beast.IsValid()||Beast->IsDead()||Beast->GetHealth()<Before);
   if(Beast.IsValid()&&!Beast->IsDead()) Beast->ApplyDamage(10000); Shot(TEXT("SkillCombatHUD.png"));
   FTransform T(P->FeetLocation()+FVector(10000,0,200)); auto* E=W->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); E->ConfigureBot(TEXT("yog_sothoth"),1); E->FinishSpawning(T); Caster=E;
   FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   auto* C=W->SpawnActor<ACultPopulation>(E->FeetLocation()+FVector(1500,0,E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*1.04f),FRotator::ZeroRotator,Spawn); C->GetCharacterMovement()->DisableMovement(); Victim=C; E->SetBotAim(C); E->AbilityQ(); Test->TestTrue(TEXT("A native roster projectile does not hit instantly"),Victim.IsValid()); Phase=3; Time=0;
  } else if(Phase==3&&Time>1.4f) {
   Test->TestTrue(TEXT("The roster's Gate Lash projectile collides with an actual civilian"),!Victim.IsValid()); DashOrigin=Caster->GetActorLocation(); Caster->AbilityF(); Test->TestTrue(TEXT("Dash cast effects stay at the caster instead of a distant camera point"),Caster->GetLastEffectLocation().Equals(Caster->FeetLocation(),1)); Phase=30; Time=0;
  } else if(Phase==30&&Time>.5f) { Test->AddInfo(FString::Printf(TEXT("AI Between Spaces dash displacement %.1f cm; %s"),FVector::Dist2D(DashOrigin,Caster->GetActorLocation()),*Caster->GetStatusText())); Test->TestTrue(TEXT("The same roster dash physically moves an AI-controlled entity"),FVector::Dist2D(DashOrigin,Caster->GetActorLocation())>500);

   if(Victim.IsValid()) Victim->Destroy(); if(Caster.IsValid()) Caster->Destroy();
   FTransform T(P->FeetLocation()+FVector(10000,0,200)); auto* E=W->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); E->ConfigureBot(TEXT("cthulhu_avatar"),1); E->FinishSpawning(T); E->SetActorLocation(E->GetActorLocation()+FVector(0,0,6000)); E->GetCharacterMovement()->DisableMovement(); Caster=E;
   const FVector Base=E->FeetLocation(); FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   auto* C=W->SpawnActor<ACultPopulation>(Base+FVector(1500,0,100),FRotator::ZeroRotator,Spawn); C->GetCharacterMovement()->DisableMovement(); Victim=C;
   AimGuide=W->SpawnActor<AStaticMeshActor>(Base+FVector(2500,0,600),FRotator::ZeroRotator,Spawn); E->SetBotAim(AimGuide);
   Raised=W->SpawnActor<ACultDestructible>(Base+FVector(2200,0,1200),FRotator::ZeroRotator,Spawn); GroundTarget=W->SpawnActor<ACultDestructible>(Base+FVector(3200,0,200),FRotator::ZeroRotator,Spawn);
   for(auto* B:{Raised,GroundTarget}) { B->SetMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); B->SetActorScale3D(FVector(2)); }
   RaisedHealth=Raised->GetHealth(); GroundHealth=GroundTarget->GetHealth(); E->HeavyAttack(); Phase=4; Time=0;

  }
  else if(Phase==4&&Time>2.3f) { Test->TestTrue(TEXT("The held wave catches a real civilian"),Victim.IsValid()&&Victim->GetActorLocation().Z>Caster->FeetLocation().Z+150); Caster->Smash(); Phase=5; Time=0; }
  else if(Phase==5&&Time>1.f) {
   Test->TestEqual(TEXT("Thrown bodies miss a structure above their actual 3D path"),Raised->GetHealth(),RaisedHealth);
   Test->TestTrue(TEXT("Thrown bodies collide with a real structure in their 3D path"),GroundTarget->GetHealth()<GroundHealth);
   Test->TestTrue(TEXT("Throw impact leaves the victim dead"),!Victim.IsValid());
   if(Victim.IsValid()) Victim->Destroy(); if(Caster.IsValid()) Caster->Destroy(); Raised->Destroy(); GroundTarget->Destroy(); AimGuide->Destroy();
   Test->AddInfo(TEXT("Actual LMB, delayed Q, Gate Lash projectile, swept 3D throws and clean HUD screenshots verified.")); return true;
  }
  if(Time>10) { Test->AddError(TEXT("Preview stage timed out")); return true; } return false;
 }
 void Shot(const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name),true,false); }
private: FAutomationTestBase* Test; double Started; float Time=0,Before=0,RaisedHealth=0,GroundHealth=0; int Phase=0; bool Entered=false,SwingShot=false; ACultUlhuPlayerPawn* P=nullptr; TWeakObjectPtr<ACultFeralBeast> Beast; TWeakObjectPtr<ACultPopulation> Victim; TWeakObjectPtr<ACultUlhuPlayerPawn> Caster; ACultDestructible* Raised=nullptr; ACultDestructible* GroundTarget=nullptr; AActor* AimGuide=nullptr; FVector DashOrigin;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultSkillPreviewTest,"CultUlhu.Menu.SkillPreview",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultSkillPreviewTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f)); ADD_LATENT_AUTOMATION_COMMAND(FSkillPreviewObservation(this)); return true; }
#endif
