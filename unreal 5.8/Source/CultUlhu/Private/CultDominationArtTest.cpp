#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultDominationArt.h"
#include "CultDominationRoute.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultSkillAttack.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
class FCultDominationArtObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0,Side=1,Shot=0,PerchIndex=0; float At=0; double Began=FPlatformTime::Seconds();
 TWeakObjectPtr<ACultMatchUnit> Target; TWeakObjectPtr<ACameraActor> Camera; bool ShowHUD=true;
 struct FSnapshot { TWeakObjectPtr<UPrimitiveComponent> Mesh; FTransform Transform; ECollisionEnabled::Type Collision; };
 TArray<FSnapshot> Geometry;
public:
 explicit FCultDominationArtObservation(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>150) { Test->AddError(TEXT("Domination art/traversal verification timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(ECultPlayMode::Domination); G->StartPlay(); Phase=1; } return false;
  }
  auto* M=ACultMatchDirector::Find(W); auto* PC=W->GetFirstPlayerController(); auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr;
  if(!M||!P||W->GetTimeSeconds()<1) return false;
  if(Phase==1) {
   M->Tick(20); M->SetActorTickEnabled(false);
   for(const auto& F:M->Fighters()) if(auto* H=F.Pawn.Get()) if(H!=P) { H->GetCharacterMovement()->DisableMovement(); H->SetActorTickEnabled(false); }
   int Landmarks=0,Skins=0,Triangles=0,Overlooks=0;
   for(TActorIterator<AActor> It(W);It;++It) {
    Overlooks+=It->ActorHasTag(TEXT("CultDominationOverlook"));
    if(It->ActorHasTag(TEXT("CultDominationArt"))) {
     auto* Mesh=It->FindComponentByClass<UProceduralMeshComponent>();
     if(It->ActorHasTag(TEXT("CultDominationKeepSkin"))||It->ActorHasTag(TEXT("CultDominationRouteInlays"))) { ++Skins; Test->TestEqual(TEXT("Facade/inlay detail never blocks movement or attacks"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision); }
     else { ++Landmarks; Test->TestEqual(TEXT("Substantial skyline geometry has real solid collision"),Mesh->GetCollisionEnabled(),ECollisionEnabled::QueryAndPhysics); Test->TestTrue(TEXT("Solid skyline stays on the existing perimeter, away from playable approaches"),FMath::Abs(It->GetActorLocation().Y)==30000&&Mesh->Bounds.GetBox().Min.Z>8000); }
     if(It->ActorHasTag(TEXT("CultDominationKeepSkin"))) Test->TestTrue(TEXT("Keep facade is placed on its actual keep"),FMath::Abs(It->GetActorLocation().X)==10500&&FMath::Abs(It->GetActorLocation().Y)==12500);
     for(int S=0;S<Mesh->GetNumSections();++S) if(auto* Section=Mesh->GetProcMeshSection(S)) Triangles+=Section->ProcIndexBuffer.Num()/3;
    }
    if(It->ActorHasTag(TEXT("MatchArena"))) { TInlineComponentArray<UPrimitiveComponent*> Parts(*It); for(auto* Mesh:Parts) Geometry.Add({Mesh,Mesh->GetComponentTransform(),Mesh->GetCollisionEnabled()}); }
   }
   Test->TestEqual(TEXT("Both balanced forward overlooks exist"),Overlooks,2); Test->TestEqual(TEXT("Distinct north/south skyline landmarks"),Landmarks,2); Test->TestEqual(TEXT("Merged keep skins and route inlay"),Skins,5);
   Test->TestTrue(TEXT("Original architecture remains a bounded low-poly addition"),Triangles>1000&&Triangles<12000); Test->AddInfo(FString::Printf(TEXT("Domination decoration: 7 merged components, %d triangles"),Triangles));
   P->RelocateMatch(FVector(0,12500,6000)); At=W->GetTimeSeconds(); Phase=2; return false;
  }
  if(Phase==2) {
   const FVector Goal(0,Side*6800,6000); P->MoveMatchToward(Goal,.016f);
   if(FVector::Dist2D(P->FeetLocation(),Goal)>120||FMath::Abs(P->FeetLocation().Z-6000)>100) {
    if(W->GetTimeSeconds()-At<12) return false;
    Test->AddError(FString::Printf(TEXT("Overlook walk failed on side %d at %s"),Side,*P->FeetLocation().ToCompactString())); return true;
   }
   Test->TestTrue(TEXT("Actual full-height hero walks from bridge onto the overlook"),P->GetCharacterMovement()->IsMovingOnGround());
   Test->TestTrue(TEXT("Overlook supplies contextual route guidance"),M->Hint(P).Contains(TEXT("SEAL OVERLOOK")));
   Test->TestFalse(TEXT("Upper players cannot capture from the overlook"),M->InsideCapture(P->FeetLocation()));
   Test->TestFalse(TEXT("Overlook slab supplies real cover below it"),ACultSkillAttack::Clear(W,FVector(0,Side*8500,500),FVector(0,Side*8500,8000),nullptr,nullptr));
   const FVector Origin=P->GetPawnViewLocation();
   const FVector Aim(0,Side*2500,200);
   Test->TestTrue(TEXT("Forward lip has an unobstructed physical firing line to the seal edge"),ACultSkillAttack::Clear(W,Origin,Aim,P,nullptr));
   const FTransform T(Aim); auto* Unit=W->SpawnActorDeferred<ACultMatchUnit>(ACultMatchUnit::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Unit->Team=1-P->GetFaction(); Unit->Health=Unit->MaxHealth=100; Unit->FinishSpawning(T); Unit->SetActorTickEnabled(false); Target=Unit;
   auto* Bolt=W->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),FTransform(Origin),P,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Bolt->Direction=(Aim-Origin).GetSafeNormal(); Bolt->Speed=9000; Bolt->Reach=12000; Bolt->Radius=45; Bolt->Windup=.12f; Bolt->Duration=2;
   TWeakObjectPtr<ACultUlhuPlayerPawn> Owner(P); Bolt->Hit=[Owner](AActor* Other,FVector Point) { if(Owner.IsValid()) ACultSkillAttack::Hurt(Other,10,Owner.Get(),Point); }; Bolt->FinishSpawning(FTransform(Origin));
   At=W->GetTimeSeconds(); Phase=3; return false;
  }
  if(Phase==3&&W->GetTimeSeconds()-At>1.8f) {
   Test->TestTrue(TEXT("A real non-homing projectile from the overlook contacts an enemy on the seal"),Target.IsValid()&&Target->Health<100);
   if(Target.IsValid()) Target->Destroy();
   P->RelocateMatch(FVector(0,Side*10200,0)); At=W->GetTimeSeconds(); Phase=7; return false;
  }
  if(Phase==7) {
   const FVector Goal(0,Side*6100,0); P->MoveMatchToward(Goal,.016f);
   if(FVector::Dist2D(P->FeetLocation(),Goal)>180||FMath::Abs(P->FeetLocation().Z)>100) {
    if(W->GetTimeSeconds()-At<10) return false;
    Test->AddError(FString::Printf(TEXT("Covered ground walk failed on side %d at %s"),Side,*P->FeetLocation().ToCompactString())); return true;
   }
   Test->TestTrue(TEXT("Full-height hero walks beneath the overlook without clipping or jumping to its roof"),P->GetCharacterMovement()->IsMovingOnGround()&&FMath::Abs(P->FeetLocation().Z)<100);
   if(Side>0) { Side=-1; P->RelocateMatch(FVector(0,-12500,6000)); At=W->GetTimeSeconds(); Phase=2; return false; }
   P->RelocateMatch(FCultDominationRoute::PerchRamp(-1,1)); At=W->GetTimeSeconds(); Phase=8; return false;
  }
  if(Phase==8) {
   const int Home=PerchIndex%2?1:-1,Flank=PerchIndex<2?1:-1;
   const FVector Goal=FCultDominationRoute::Perch(Home,Flank); P->MoveMatchToward(Goal,.016f);
   if(FVector::Dist2D(P->FeetLocation(),Goal)>180||FMath::Abs(P->FeetLocation().Z-Goal.Z)>100) {
    if(W->GetTimeSeconds()-At<10) return false;
    Test->AddError(FString::Printf(TEXT("Full-height perch ramp %d failed at %s"),PerchIndex,*P->FeetLocation().ToCompactString())); return true;
   }
   Test->TestTrue(TEXT("Full-height hero walks onto each low firing perch"),P->GetCharacterMovement()->IsMovingOnGround());
   Test->TestFalse(TEXT("Low firing perches do not capture the seal"),M->InsideCapture(P->FeetLocation()));
   if(++PerchIndex<4) { P->RelocateMatch(FCultDominationRoute::PerchRamp(PerchIndex%2?1:-1,PerchIndex<2?1:-1)); At=W->GetTimeSeconds(); return false; }
   P->RelocateMatch(M->Base(0)+FVector(2500,0,0)); Camera=W->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetConstraintAspectRatio(false); Camera->GetCameraComponent()->SetFieldOfView(75); Camera->GetCameraComponent()->PostProcessBlendWeight=0; PC->SetViewTarget(Camera.Get());
   if(auto* HUD=PC->GetHUD()) { ShowHUD=HUD->bShowHUD; HUD->bShowHUD=false; } Phase=4;
  }
  if(Phase==4) {
   const int View=Shot/2; const FVector Sites[]={FVector(-18000,-7000,11000),FVector(-18000,7000,11000),FVector(-18500,12500,6000),FVector(1500,8500,9500),FVector(-9000,2000,6500),FVector(9000,-2000,6500)};
   const FVector Goals[]={FVector(0,30000,14000),FVector(0,-30000,14000),FVector(0,12500,-3500),FVector(0,0,1600),FVector(-3300,5000,900),FVector(3300,-5000,900)};
   CultDominationArt::Compare(W,Shot%2!=0); Camera->SetActorLocation(Sites[View]); Camera->SetActorRotation((Goals[View]-Sites[View]).Rotation()); PC->PlayerCameraManager->UpdateCamera(0);
   for(const auto& Old:Geometry) if(auto* Mesh=Old.Mesh.Get()) { Test->TestTrue(TEXT("Appearance comparison preserves all geometry transforms"),Mesh->GetComponentTransform().Equals(Old.Transform,.01f)); Test->TestEqual(TEXT("Appearance comparison preserves physical collision"),Mesh->GetCollisionEnabled(),Old.Collision); }
   At=W->GetTimeSeconds(); Phase=5; return false;
  }
  if(Phase==5&&W->GetTimeSeconds()-At>.8f) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/DominationArt_%d_%s.png"),Shot/2,Shot%2?TEXT("After"):TEXT("Before")),false,false); At=W->GetTimeSeconds(); Phase=6; return false; }
  if(Phase==6&&W->GetTimeSeconds()-At>.3f) {
   if(++Shot<12) { Phase=4; return false; }
   CultDominationArt::Compare(W,true); PC->SetViewTarget(P); if(auto* HUD=PC->GetHUD()) HUD->bShowHUD=ShowHUD; Camera->Destroy(); return true;
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDominationArtTest,"CultUlhu.Menu.DominationArchitecture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDominationArtTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultDominationArtObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
