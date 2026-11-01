#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "CultDominationRoute.h"
#include "CultSkillAttack.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

// Fixture placement is confined to scenario setup. Thereafter the normal match
// brain, character movement, original kit and physical projectiles do the work.
class FCultDominationUpperObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0,Scenario=0,Slot=1,Team=0,Side=1;
 float At=0,HealthOnPerch=0,Highest=0,NextLog=0; bool Perch=false,Bolt=false,Hit=false,Exiting=false;
 TSet<TWeakObjectPtr<ACultSkillAttack>> Observed;
 TSharedPtr<bool> RangedContact=MakeShared<bool>(false);
 double Began=FPlatformTime::Seconds();
public:
 explicit FCultDominationUpperObservation(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>230) { Test->AddError(TEXT("Autonomous upper-route observation timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(ECultPlayMode::Domination); G->StartPlay(); Phase=1; } return false;
  }
  auto* M=ACultMatchDirector::Find(W); if(!M||W->GetTimeSeconds()<1) return false;
  auto& Fighters=const_cast<TArray<FCultMatchFighter>&>(M->Fighters());
  if(Fighters.Num()!=10) return false;
  if(Phase==1) {
   M->Tick(20);
   for(auto& F:Fighters) {
    auto* P=F.Pawn.Get(); if(!P) { Test->AddError(TEXT("Missing fixture fighter")); return true; }
    P->ConfigurePlayer(F.Kit,F.Team); P->RelocateMatch(M->Base(F.Team)+FVector(0,(F.Lane-1)*1800,0)); P->GetCharacterMovement()->DisableMovement(); P->HealMatch(P->GetMaxHealth());
    F.Shield=0; F.Respawn=0; F.Retreating=false; F.DomPath.Reset(); F.RouteStep=2; F.Think=0; F.NextTacticalMove=9999;
   }
   Slot=Scenario==0?1:Scenario==1?6:2; Team=Scenario==1?1:0; Side=Scenario==1?-1:1;
   Fighters[Team*5].Pawn->RelocateMatch(FVector(0,0,0));
   if(Scenario==2) { Fighters[3].Pawn->RelocateMatch(FVector(1000,-1000,0)); M->Tick(60); Test->TestTrue(TEXT("High transit fixture has a genuine score lead"),M->Score(0)>=M->Score(1)+25); }
   auto& F=Fighters[Slot]; F.Pawn->Destroy(); F.Kit=TEXT("mi_go");
   const FTransform Spawn(M->Base(Team)+FVector(0,0,1500));
   auto* NewBot=W->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   NewBot->ConfigureBot(F.Kit,Team); F.Pawn=NewBot; NewBot->FinishSpawning(Spawn); NewBot->RespawnMatch(M->Base(Team));
   Test->TestEqual(TEXT("Fixture spawns the actual authored Mi-Go kit"),NewBot->BotRangedReach(),2000.f);
   // Do not supply a route or movement commands to this bot.
   // Outside Mi-Go's physical arm reach, inside its original projectile reach.
   Fighters[(1-Team)*5].Pawn->RelocateMatch(FVector((Scenario==2?1:Team?1:-1)*3300,Side*2300,0));
   Perch=Bolt=Hit=Exiting=false; Observed.Reset(); RangedContact=MakeShared<bool>(false); Highest=0; At=W->GetTimeSeconds(); NextLog=At+5; Phase=2; return false;
  }
  auto* P=Fighters[Slot].Pawn.Get(); auto* Target=Fighters[(1-Team)*5].Pawn.Get(); const FVector Feet=P->FeetLocation(); Highest=FMath::Max(Highest,float(Feet.Z));
  if(Phase==2) {
   // Competitive heroes have 100 HP: legitimate approach/melee hits can kill
   // this fixture before the ranged observation. Heal between frames until an
   // instrumented original projectile actually deals damage from the perch.
   if(!*RangedContact) Target->HealMatch(Target->GetMaxHealth());
   const FVector Site=FCultDominationRoute::Perch(Scenario==2?1:Team?1:-1,Side);
   if(W->GetTimeSeconds()>NextLog) { Test->AddInfo(FString::Printf(TEXT("Scenario %d at %.1fs: feet=%s goal=%s"),Scenario,W->GetTimeSeconds()-At,*Feet.ToCompactString(),*Fighters[Slot].TacticalGoal.ToCompactString())); NextLog+=5; }
   const bool Here=FVector::Dist2D(Feet,Site)<450&&FMath::Abs(Feet.Z-Site.Z)<120;
   if(Here&&!Perch) { Perch=true; HealthOnPerch=Target->GetHealth(); Test->AddInfo(FString::Printf(TEXT("Scenario %d: autonomous Mi-Go reached low perch after %.2fs, highest feet %.1f"),Scenario,W->GetTimeSeconds()-At,Highest)); }
   if(Here) {
    for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==P&&It->Shape==ACultSkillAttack::EShape::Bolt&&It->Reach==2000.f) {
     Bolt=true; if(Observed.Contains(*It)) continue; Observed.Add(*It);
     auto Original=MoveTemp(It->Hit); TWeakObjectPtr<ACultUlhuPlayerPawn> Victim(Target),Shooter(P); auto Contact=RangedContact;
     It->Hit=[Original=MoveTemp(Original),Victim,Shooter,Contact,Site](AActor* Other,FVector Point) mutable {
      const float Before=Victim.IsValid()?Victim->GetHealth():0;
      if(Original) Original(Other,Point);
      if(Other==Victim.Get()&&Victim.IsValid()&&Shooter.IsValid()&&Victim->GetHealth()<Before&&FVector::Dist2D(Shooter->FeetLocation(),Site)<500&&FMath::Abs(Shooter->FeetLocation().Z-Site.Z)<150) *Contact=true;
     };
    }
    Hit|=*RangedContact;
   }
   if(Hit) {
    Test->TestTrue(TEXT("Autonomous original Mi-Go kit physically damages an enemy from the perch"),Perch&&Bolt&&Hit);
    if(Scenario==2) Test->TestTrue(TEXT("Bot physically climbed the high crossing before dropping to useful range"),Highest>5800);
    // Moving a fixture target removes the opportunity; the bot must leave on its own.
    if(Scenario==1) P->ReceiveDamage(P->GetMaxHealth()*.8f);
    else Target->RelocateMatch(M->Base(1-Team));
    At=W->GetTimeSeconds(); Phase=3; return false;
   }
   if(W->GetTimeSeconds()-At>(Scenario==2?80:35)) {
    Test->AddError(FString::Printf(TEXT("Scenario %d failed real-kit perch pressure: feet=%s goal=%s highest=%.1f perch=%d bolt=%d health=%.1f baseline=%.1f"),Scenario,*Feet.ToCompactString(),*Fighters[Slot].TacticalGoal.ToCompactString(),Highest,Perch,Bolt,Target->GetHealth(),HealthOnPerch)); return true;
   }
  }
  if(Phase==3) {
   Exiting|=FVector::Dist2D(Feet,FCultDominationRoute::Perch(Scenario==2?1:Team?1:-1,Side))>1200;
   if(Exiting&&FMath::Abs(Feet.Z)<100) {
    Test->TestTrue(TEXT("Bot physically returns to ground when pressure ends"),true);
    if(Scenario==1) Test->TestTrue(TEXT("Low health causes retreat rather than another perch attack"),Fighters[Slot].Retreating);
    if(++Scenario==3) return true;
    Phase=1; return false;
   }
   if(W->GetTimeSeconds()-At>18) { Test->AddError(FString::Printf(TEXT("Scenario %d bot failed to leave perch: %s -> %s"),Scenario,*Feet.ToCompactString(),*Fighters[Slot].TacticalGoal.ToCompactString())); return true; }
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDominationUpperTest,"CultUlhu.Menu.DominationUpperCombat",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDominationUpperTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultDominationUpperObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
