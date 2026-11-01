#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultSkillAttack.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

class FCultObjectiveCollisionObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Kind,Phase=0; double Started=FPlatformTime::Seconds(); float At=0,WalkX=0,DashX=0,MaxDetour=0,HeroDetour=0;
 bool HeroReached=false;
 TWeakObjectPtr<ACultMatchUnit> Objective,Minion;
public:
 FCultObjectiveCollisionObservation(FAutomationTestBase* In,int Type):Test(In),Kind(Type) {}
 ~FCultObjectiveCollisionObservation() { if(Objective.IsValid()) Objective->Destroy(); if(Minion.IsValid()) Minion->Destroy(); }
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>100) { Test->AddError(TEXT("Objective collision check timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(ECultPlayMode::Dotgoo); G->OpenBeliefSelect(); Phase=1; } return false;
  }
  auto* M=ACultMatchDirector::Find(W); if(!M||W->GetTimeSeconds()<1) return false;
  auto* P=M->Fighters()[0].Pawn.Get(); if(!P) return false;
  if(Phase==1) {
   M->Tick(20); M->Tick(4); M->SetActorTickEnabled(false);
   for(int I=1;I<10;++I) if(auto* H=M->Fighters()[I].Pawn.Get()) { H->GetCharacterMovement()->DisableMovement(); H->SetActorTickEnabled(false); }
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) It->Destroy();
   for(TActorIterator<ACultMatchUnit> It(W);It;++It) It->SetActorTickEnabled(false);
   const float Half=Kind==4?1600:1250; const FTransform T(FVector(0,0,Half+3));
   Objective=W->SpawnActorDeferred<ACultMatchUnit>(ACultMatchUnit::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Objective->Kind=Kind; Objective->Team=0; Objective->Health=3000; Objective->Range=0; Objective->Dps=0; Objective->FinishSpawning(T);
   P->RespawnMatch(FVector(-3500,0,0)); At=W->GetTimeSeconds(); Phase=2; return false;
  }
  if(Phase==2) {
   // Raw character input deliberately excludes the bot's proactive steering:
   // the live movement component itself must stop at the objective.
   P->AddMovementInput(FVector::ForwardVector,1,true);
   if(W->GetTimeSeconds()-At<3.5f) return false;
   WalkX=P->GetActorLocation().X;
   Test->TestTrue(TEXT("Walking cannot cross the solid objective centre"),WalkX< -P->GetSimpleCollisionRadius());
   auto* Cap=Objective->FindComponentByClass<UCapsuleComponent>();
   P->RespawnMatch(FVector(-P->GetSimpleCollisionRadius()-Cap->GetScaledCapsuleRadius()-300,0,0));
   Test->TestTrue(TEXT("A real directional dash starts outside the objective"),P->TryDash(FVector::ForwardVector)); At=W->GetTimeSeconds(); Phase=3; return false;
  }
  if(Phase==3) {
   if(W->GetTimeSeconds()-At<.6f) return false;
   DashX=P->GetActorLocation().X;
   Test->TestTrue(TEXT("Root-motion dash cannot pass through the objective"),DashX< -P->GetSimpleCollisionRadius());
   P->RelocateMatch(FVector(-3500,0,0));
   const FTransform T(FVector(-2500,0,153));
   Minion=W->SpawnActorDeferred<ACultMatchUnit>(ACultMatchUnit::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Minion->Kind=0; Minion->Team=0; Minion->Health=120; Minion->Range=0; Minion->Dps=0; Minion->Speed=1200; Minion->FinishSpawning(T);
   At=W->GetTimeSeconds(); Phase=4; return false;
  }
  if(Phase==4) {
   if(!HeroReached) P->MoveMatchToward(FVector(3500,0,0),W->GetDeltaSeconds());
   HeroDetour=FMath::Max(HeroDetour,float(FMath::Abs(P->GetActorLocation().Y)));
   HeroReached|=FVector::Dist2D(P->FeetLocation(),FVector(3500,0,0))<300;
   Minion->MoveTo(FVector(2500,0,153),W->GetDeltaSeconds()); MaxDetour=FMath::Max(MaxDetour,float(FMath::Abs(Minion->GetActorLocation().Y)));
   if((FVector::Dist2D(Minion->GetActorLocation(),FVector(2500,0,153))>180||!HeroReached)&&W->GetTimeSeconds()-At<9) return false;
   Test->TestTrue(TEXT("An allied minion physically navigates past its own objective"),Minion->GetActorLocation().X>2200);
   Test->TestTrue(TEXT("The minion walks around the objective rather than through its centre"),MaxDetour>250);
   Test->TestTrue(TEXT("Full-size hero locomotion reaches the far side around the solid objective"),HeroReached&&HeroDetour>P->GetSimpleCollisionRadius());
   Test->AddInfo(FString::Printf(TEXT("Objective collision kind=%d walkX=%.1f dashX=%.1f minionX=%.1f detour=%.1f"),Kind,WalkX,DashX,Minion->GetActorLocation().X,MaxDetour));
   Test->AddInfo(FString::Printf(TEXT("Hero objective detour kind=%d reached=%d maxY=%.1f final=%s"),Kind,int(HeroReached),HeroDetour,*P->GetActorLocation().ToCompactString()));
   Minion->Destroy(); Objective->Destroy(); P->RespawnMatch(FVector(-3500,0,0)); At=W->GetTimeSeconds(); Phase=5; return false;
  }
  P->AddMovementInput(FVector::ForwardVector,1,true);
  if(W->GetTimeSeconds()-At<3.5f) return false;
  Test->TestTrue(TEXT("Removing the objective clears its blocking body from the route"),P->GetActorLocation().X>1000);
  Test->AddInfo(FString::Printf(TEXT("Objective removed kind=%d freeWalkX=%.1f playerHeight=%.1f"),Kind,P->GetActorLocation().X,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*2)); return true;
 }
};
#define CULT_OBJECTIVE_COLLISION_TEST(Type,Path,Kind) \
 IMPLEMENT_SIMPLE_AUTOMATION_TEST(Type,Path,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
 bool Type::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultObjectiveCollisionObservation(this,Kind)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
CULT_OBJECTIVE_COLLISION_TEST(FCultLaneWardCollisionTest,"CultUlhu.Menu.ObjectiveCollision.LaneWard",3)
CULT_OBJECTIVE_COLLISION_TEST(FCultNexusWardCollisionTest,"CultUlhu.Menu.ObjectiveCollision.NexusWard",5)
CULT_OBJECTIVE_COLLISION_TEST(FCultGreatOldOneCollisionTest,"CultUlhu.Menu.ObjectiveCollision.GreatOldOne",4)
#undef CULT_OBJECTIVE_COLLISION_TEST
#endif
