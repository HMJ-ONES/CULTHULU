#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CultSkillAttack.h"
#include "CultCorpse.h"
#include "CultWorldDirector.h"
#include "core/Events.h"
#include "CultPopulation.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuGameInstance.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#endif
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"

class FSkillCombatObservation:public IAutomationLatentCommand {
public:
 explicit FSkillCombatObservation(FAutomationTestBase* T):Test(T),Started(FPlatformTime::Seconds()) {}
 bool Update() override {
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) { Test->AddError(TEXT("Dark Wilds game world required")); return true; }
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Entered) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(ECultPlayMode::FreeRoam); G->SetInitialBeliefs({11,5,6}); G->StartPlay(); Entered=true; } if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Combat test never entered the world")); return true; } return false; }
  if(!W->GetName().Contains(TEXT("L_DarkWilds"))||W->GetTimeSeconds()<2) return false;
  Clock+=W->GetDeltaSeconds();
  if(Phase==0) {
   for(TActorIterator<ACultUlhuPlayerPawn> It(W);It;++It) if(!It->IsBot()) Player=*It;
   if(!Player) { Test->AddError(TEXT("No playable pawn")); return true; }
   // Keep the collision fixture above all procedural terrain and city structures.
   Origin=Player->FeetLocation()+FVector(15000,-15000,50000);
   FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Target=W->SpawnActor<ACultPopulation>(Origin+FVector(1200,0,0),FRotator::ZeroRotator,Params); Target->GetCharacterMovement()->DisableMovement();
   Wall=W->SpawnActor<AStaticMeshActor>(Origin+FVector(600,0,0),FRotator::ZeroRotator,Params); Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); Wall->SetActorScale3D(FVector(1,8,8)); Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
   Bolt(W); Phase=1; Clock=0; return false;
  }
  if(Phase==1&&Clock>.8f) { Test->TestEqual(TEXT("Solid cover blocks the physical projectile"),Target->GetHealth(),1.f); Wall->Destroy(); Bolt(W); Phase=2; Clock=0; }
  else if(Phase==2&&Clock>.15f) { Target->SetActorLocation(Origin+FVector(1200,800,0)); Phase=3; }
  else if(Phase==3&&Clock>.8f) { Test->TestEqual(TEXT("A projectile misses a target that sidesteps its locked trajectory"),Target->GetHealth(),1.f); Target->SetActorLocation(Origin+FVector(1200,0,0)); Bolt(W); Test->TestEqual(TEXT("Projectile damage is not instantaneous"),Target->GetHealth(),1.f); Phase=4; Clock=0; }
  else if(Phase==4&&Clock>.8f) { Test->TestTrue(TEXT("An actual projectile contact kills a civilian in one hit"),!Target.IsValid()); int Corpses=0; for(TActorIterator<ACultCorpse> It(W);It;++It) ++Corpses; InitialCorpses=Corpses; Test->TestTrue(TEXT("Civilian death leaves a corpse"),Corpses>0);
   Test->TestTrue(TEXT("A directional dash is available"),Player->TryDash(FVector(0,1,0))); Before=Player->GetHealth(); Player->ReceiveDamage(20); Test->TestEqual(TEXT("Dash evade window rejects incoming damage"),Player->GetHealth(),Before); Test->TestFalse(TEXT("Holding cannot repeat a dash during cooldown"),Player->TryDash(FVector(0,-1,0))); Phase=5; Clock=0;
  }
  else if(Phase==5&&Clock>.35f) { Player->ReceiveDamage(10); Test->TestTrue(TEXT("Dash protection expires"),Player->GetHealth()<Before); Before=Player->GetHealth(); const FVector Feet=Player->FeetLocation();
   // Measure this particular attack's physical contacts. Ambient creatures remain
   // active in Free Roam and may land unrelated hits during the observation.
   GroundCaster=nullptr; for(TActorIterator<ACultUlhuPlayerPawn> Rival(W);Rival;++Rival) if(Rival->IsBot()&&ACultSkillAttack::Alive(*Rival)) { GroundCaster=*Rival; break; }
   Test->TestNotNull(TEXT("Ground fixture has a living hostile caster"),GroundCaster);
   GroundAttack(W,Feet); Phase=9; Clock=0; }
  else if(Phase==9&&Clock>.45f) {
   // A real world tick must advance the standing control. Manually ticking a
   // just-born attack is intentionally clamped by its first-frame safeguard.
   Test->TestEqual(TEXT("The same low attack actually contacts a standing hero"),GroundContacts,1);
   GroundContacts=0; Before=Player->GetHealth(); Player->TryJump(); GroundAttack(W,Player->FeetLocation()); Phase=6; Clock=0; }
  else if(Phase==6&&Clock>3.5f) { int N=0; for(TActorIterator<ACultCorpse> It(W);It;++It) ++N; Test->TestTrue(TEXT("Corpses remain after the old three-second cleanup window"),N>=InitialCorpses); Test->TestEqual(TEXT("Jumping clears the delayed low attack collider"),GroundContacts,0); Test->AddInfo(FString::Printf(TEXT("Jump fixture: %d contacts from its ground attack; total health change %.1f"),GroundContacts,Before-Player->GetHealth()));
   FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Target=W->SpawnActor<ACultPopulation>(Origin+FVector(1200,0,0),FRotator::ZeroRotator,Params); Target->RecruitTo(2); Target->GetCharacterMovement()->DisableMovement();
   FTransform T(Origin); auto* A=W->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,Player,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); A->Shape=ACultSkillAttack::EShape::Sweep; A->Direction=FVector::ForwardVector; A->Reach=1500; A->Radius=25; A->Windup=.2f; A->Duration=.25f; A->Hit=[this](AActor* Actor,FVector Point) { ++MeleeContacts; ACultSkillAttack::Hurt(Actor,10,Player,Point); }; A->FinishSpawning(T); Phase=7; Clock=0; }
  else if(Phase==7&&Clock<.15f) Test->TestEqual(TEXT("Arm windup has no instant damage"),Target->GetHealth(),100.f);
  else if(Phase==7&&Clock>.7f) { Test->TestEqual(TEXT("Swept arm deals damage once per target"),MeleeContacts,1); Test->TestEqual(TEXT("A slash makes one actual contact"),Target->GetHealth(),90.f); FTransform T(Target->GetActorLocation()); auto* A=W->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,Target.Get(),nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); A->Shape=ACultSkillAttack::EShape::Sweep; A->Windup=1; A->Duration=.2f; A->FinishSpawning(T); Pending=A; Target->SetControlState(true,false,0,Target->GetActorLocation(),true,2026); Phase=8; Clock=0; }
  else if(Phase==8&&Clock>.2f) { Test->TestFalse(TEXT("Stunning a fighter cancels its unfinished melee"),Pending.IsValid()); Target->Destroy(); Bolt(W); Player->ReceiveDamage(100000); Test->TestEqual(TEXT("A defeated vessel reforms with its source health"),Player->GetHealth(),Player->GetMaxHealth()); int Active=0; for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==Player) ++Active; Test->TestEqual(TEXT("Defeat cancels attacks instead of sweeping them across a respawn teleport"),Active,0); auto* Director=ACultWorldDirector::Find(W); const int Slain=Director->FaithEventCount(int(cultulhu::EventType::CivilianSlain)); FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn; auto* Former=W->SpawnActor<ACultPopulation>(Origin+FVector(1800,0,0),FRotator::ZeroRotator,Params); Former->RecruitTo(2); Former->Desert(); Former->ReceiveAttack(1,0,true); Test->TestFalse(TEXT("A former cultist is still a one-hit civilian"),IsValid(Former)); Test->TestEqual(TEXT("A forced civilian kill awards its actual belief event once"),Director->FaithEventCount(int(cultulhu::EventType::CivilianSlain)),Slain+1); Test->AddInfo(TEXT("Live checks: cover, projectile travel, sidestep, contact, jump clearance, corpse persistence, slash timing, dash timing.")); return true; }
  if(Clock>10) { Test->AddError(TEXT("Skill combat observation timed out")); return true; } return false;
 }
 ACultSkillAttack* Area(UWorld* W,FVector At,ACultSkillAttack::EShape Kind) { FTransform T(At); auto* A=W->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,Player,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); A->Shape=Kind; A->Reach=300; A->Radius=120; A->Windup=.3f; A->Duration=.05f; A->Hit=[this](AActor* Actor,FVector Point) { ACultSkillAttack::Hurt(Actor,40,Player,Point); }; A->FinishSpawning(T); return A; }
 void GroundAttack(UWorld* W,FVector Feet) { auto* A=Area(W,Feet,ACultSkillAttack::EShape::GroundDisk); A->SetOwner(GroundCaster); A->Radius=120; A->Reach=Player->GetSimpleCollisionRadius()+300;
  A->Hit=[this](AActor* Actor,FVector Point) { if(Actor==Player) ++GroundContacts; ACultSkillAttack::Hurt(Actor,40,GroundCaster,Point); }; }
 void Bolt(UWorld* W) { FTransform T(Origin); auto* A=W->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,Player,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); A->Shape=ACultSkillAttack::EShape::Bolt; A->Direction=FVector::ForwardVector; A->Speed=3000; A->Reach=1800; A->Radius=20; A->Windup=.1f; A->Duration=.7f; TWeakObjectPtr<ACultUlhuPlayerPawn> P(Player); A->Hit=[P](AActor* Actor,FVector Point) { if(P.IsValid()) ACultSkillAttack::Hurt(Actor,1,P.Get(),Point); }; A->FinishSpawning(T); }
private:
 FAutomationTestBase* Test; double Started; bool Entered=false; int Phase=0,InitialCorpses=0,MeleeContacts=0,GroundContacts=0; float Clock=0,Before=0; FVector Origin; ACultUlhuPlayerPawn* Player=nullptr; AActor* GroundCaster=nullptr; TWeakObjectPtr<ACultSkillAttack> Pending; TWeakObjectPtr<ACultPopulation> Target; AStaticMeshActor* Wall=nullptr;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultSkillCombatTest,"CultUlhu.SkillCombat",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCultSkillCombatTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FSkillCombatObservation(this)); return true; }
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultSkillCombatEditorTest,"CultUlhu.Menu.SkillCombat",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultSkillCombatEditorTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f)); ADD_LATENT_AUTOMATION_COMMAND(FSkillCombatObservation(this)); return true; }
#endif
#endif
