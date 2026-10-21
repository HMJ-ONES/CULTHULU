#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "CultJungleCamp.h"
#include "CultFeralBeast.h"
#include "CultMobaJungle.h"
#include "CultMatchDirector.h"
#include "CultMatchState.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPopulation.h"
#include "CultSkillAttack.h"
#include "CultCorpse.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

struct FCultJungleTestAccess {
 static void RespawnSoon(ACultJungleCamp* Camp) { Camp->Respawn_=.6f; }
 static void Retaliate(ACultMatchDirector* M,int Index) {
  auto& F=M->State_->Fighters[Index]; F.RouteStep=2; F.Think=0;
  F.Pawn->SetActorTickEnabled(true); M->BotThink(Index,.3f);
 }
};
namespace {
ACultSkillAttack* Windup(UWorld* W,ACultFeralBeast* B) {
 for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==B&&It->IsWindingUp()) return *It;
 return nullptr;
}
void Park(ACultMatchDirector* M,ACultUlhuPlayerPawn* P) { P->RelocateMatch(M->Base(P->GetFaction())+FVector(P->GetFaction()?-2600:2600,0,0)); }
void Place(ACultMatchDirector* M,ACultUlhuPlayerPawn* P,FVector Feet) { P->RespawnMatch(Feet); M->OnAction(P); }
void Bolt(UWorld* W,AActor* Source,FVector Origin,ACultFeralBeast* B) {
 FTransform Transform(Origin); auto* Shot=W->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),Transform,Source,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 Shot->Shape=ACultSkillAttack::EShape::Bolt; Shot->Direction=(B->GetActorLocation()-Origin).GetSafeNormal(); Shot->Radius=45; Shot->Reach=6000; Shot->Speed=6000; Shot->Windup=.1f; Shot->Duration=1.2f;
 TWeakObjectPtr<AActor> Weak(Source); Shot->Hit=[Weak](AActor* Other,FVector Point) { if(Weak.IsValid()) ACultSkillAttack::Hurt(Other,500,Weak.Get(),Point); }; Shot->FinishSpawning(Transform);
}
}
class FCultJungleCampObservation:public IAutomationLatentCommand {
 FAutomationTestBase* T; int Phase=0; double Began=FPlatformTime::Seconds(); float At=0,Hp=0; int XP=0,AllyXP=0,EnemyXP=0;
 FVector DashStart,ResetStart; TWeakObjectPtr<ACultFeralBeast> Original; TWeakObjectPtr<ACultPopulation> Summon;
 ACultJungleCamp* Camps[4]={}; bool SawReset=false;
public:
 explicit FCultJungleCampObservation(FAutomationTestBase* Test):T(Test) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>100) { T->AddError(TEXT("Jungle camp verification timed out")); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(Phase==0) { Phase=1; auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->SelectedCharacterId=TEXT("cthulhu_avatar"); GI->OpenCharacterSelect(ECultPlayMode::Dotgoo); GI->OpenBeliefSelect(); } return false;
  }
  auto* M=ACultMatchDirector::Find(W); if(!M||M->Warmup()>0||M->Fighters().Num()!=10) return false;
  auto* P=M->Fighters()[0].Pawn.Get(); auto* Ally=M->Fighters()[1].Pawn.Get(); auto* Enemy=M->Fighters()[5].Pawn.Get(); const float Now=W->GetTimeSeconds();
  if(Phase==1) {
   // Freeze lane strategy for isolated encounter assertions; camps and actual
   // projectiles keep ticking. Full match coverage runs separately.
   M->SetActorTickEnabled(false);
   for(const auto& F:M->Fighters()) if(auto* Hero=F.Pawn.Get()) { M->OnAction(Hero); Park(M,Hero); Hero->GetCharacterMovement()->StopMovementImmediately(); if(Hero!=P) { Hero->GetCharacterMovement()->DisableMovement(); Hero->SetActorTickEnabled(false); } }
   FTransform BirthTransform(P->GetActorLocation()); auto* Fresh=W->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),BirthTransform,P,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Fresh->Windup=.5f; Fresh->Duration=.1f; Fresh->FinishSpawning(BirthTransform); Fresh->Tick(.333f);
   T->TestTrue(TEXT("A newly created attack cannot consume a slow frame's pre-creation time"),FMath::IsNearlyEqual(Fresh->TimeUntilActive(),.5f,.001f)); Fresh->Destroy();
   int Count=0; for(TActorIterator<ACultJungleCamp> It(W);It;++It) { Camps[It->CampIndex]=*It; ++Count; }
   T->TestEqual(TEXT("The actual MOBA world contains four neutral camps"),Count,4); if(Count!=4) return true;
   for(int I=0;I<4;++I) {
    auto* B=Camps[I]->Monster(); T->TestNotNull(TEXT("A camp owns a live creature actor"),B); if(!B) return true;
    T->TestTrue(TEXT("Jungle creatures use authored skeletal parts"),B->HasCreatureMesh()&&B->GetCreaturePartCount()>=8);
    T->TestTrue(TEXT("Neutral camp XP exceeds a lane minion's 60-XP killing blow"),Camps[I]->RewardXP()>60);
    T->TestTrue(TEXT("Neutral swings exceed minor lane damage"),B->GetAttackDamage()>=20);
    T->TestFalse(TEXT("Competitive creatures cannot be healed by heroes"),P->CanHealActor(B)); B->Capture(P,0); T->TestFalse(TEXT("A neutral MOBA camp cannot become a free-roam captive"),B->IsCaptured());
   }
   Place(M,P,FCultMobaJungle::Ground(2)+FVector(-1000,0,0)); At=Now; Phase=13; return false;
  }
  auto* C=Camps[0]; auto* B=C->Monster();
  if(Phase==2) {
   if(auto* Attack=Windup(W,B)) { T->TestEqual(TEXT("The enlarged hunter retains a quick species windup"),Attack->Windup,.4f); T->TestEqual(TEXT("The hunter's real AI swing has anticipation before damage"),P->GetHealth(),Hp); At=Now; Phase=3; return false; }
   if(Now-At>5) { T->AddError(TEXT("Hunter did not initiate a physical swing")); return true; } return false;
  }
  if(Phase==3) {
   if(P->GetHealth()<Hp) {
    T->TestTrue(TEXT("A real hunter swipe deals substantially more than minor enemy damage"),Hp-P->GetHealth()>=18&&Hp-P->GetHealth()<=20.1f);
    P->HealMatch(1000); Hp=P->GetHealth(); At=Now; Phase=4; return false;
   }
   if(Now-At>2) { T->AddError(TEXT("A committed hunter swing never hit the in-range hero")); return true; } return false;
  }
  if(Phase==4) {
   if(Windup(W,B)) { DashStart=P->FeetLocation(); T->TestTrue(TEXT("The player can use the actual lateral dash against a camp"),P->TryDash(FVector(0,1,0))); At=Now; Phase=5; return false; }
   if(Now-At>5) { T->AddError(TEXT("Hunter failed to recover for a second swing")); return true; } return false;
  }
  if(Phase==5) {
   if(Now-At<1.1f) return false;
   T->TestTrue(TEXT("Dash physically moves the player out of the committed swing"),FVector::Dist2D(P->FeetLocation(),DashStart)>500);
   T->TestTrue(TEXT("The outmanoeuvred physical swing does not automatically hit"),P->GetHealth()>=Hp-.1f);
   Place(M,P,FCultMobaJungle::Ground(0)+FVector(-1800,0,0)); ACultSkillAttack::Hurt(B,10,P,B->GetActorLocation());
   const float Half=B->FindComponentByClass<UCapsuleComponent>()->GetScaledCapsuleHalfHeight(); B->SetActorLocation(FCultMobaJungle::Ground(0)+FVector(1000,0,Half+3)); ResetStart=B->GetActorLocation();
   XP=M->Fighter(P)->XP; Park(M,P); At=Now; Phase=6; return false;
  }
  if(Phase==6) {
   if(C->Resetting()&&!SawReset) {
    SawReset=true; const float Before=B->GetHealth(); ACultSkillAttack::Hurt(B,1000,P,B->GetActorLocation());
    T->TestEqual(TEXT("Leashing cannot be exploited for a duplicate kill"),B->GetHealth(),Before);
   }
   if(SawReset&&!C->Resetting()&&FVector::Dist2D(B->GetActorLocation(),C->GetActorLocation())<130) {
    T->TestTrue(TEXT("The creature travels back to its camp"),FVector::Dist2D(ResetStart,B->GetActorLocation())>800);
    T->TestEqual(TEXT("Reset restores health"),B->GetHealth(),B->GetMaxHealth()); T->TestEqual(TEXT("Reset awards no XP"),M->Fighter(P)->XP,XP);
    Place(M,P,FCultMobaJungle::Ground(0)+FVector(-2200,0,0)); Place(M,Ally,FCultMobaJungle::Ground(0)+FVector(-600,1400,0)); Ally->GetCharacterMovement()->DisableMovement(); Place(M,Enemy,FCultMobaJungle::Ground(0)+FVector(-600,-1400,0)); Enemy->GetCharacterMovement()->DisableMovement();
    XP=M->Fighter(P)->XP; AllyXP=M->Fighter(Ally)->XP; EnemyXP=M->Fighter(Enemy)->XP; Original=B;
    Bolt(W,P,P->FeetLocation()+FVector(0,0,220),B); At=Now; Phase=7; return false;
   }
   if(Now-At>5) { T->AddError(TEXT("Hunter did not complete its leash/reset")); return true; } return false;
  }
  if(Phase==7) {
   if(C->RespawnRemaining()>0) {
    T->TestEqual(TEXT("A physical projectile kill gives its hero 180 XP"),M->Fighter(P)->XP-XP,180);
    T->TestEqual(TEXT("A nearby allied hero shares 90 XP"),M->Fighter(Ally)->XP-AllyXP,90);
    T->TestEqual(TEXT("The opposing hero receives no share"),M->Fighter(Enemy)->XP,EnemyXP);
    C->MonsterDied(Original.Get()); T->TestEqual(TEXT("Repeated death notifications cannot pay twice"),M->Fighter(P)->XP-XP,180);
    T->TestTrue(TEXT("A hunter uses its configured 60-second respawn"),C->RespawnRemaining()>55&&C->RespawnRemaining()<=60);
    Park(M,P); Park(M,Ally); Park(M,Enemy); FCultJungleTestAccess::RespawnSoon(C); At=Now; Phase=8; return false;
   }
   if(Now-At>3) { T->AddError(TEXT("The physical kill projectile failed to reach the camp creature")); return true; } return false;
  }
  if(Phase==8) {
   if(B&&!B->IsDead()&&B!=Original.Get()) {
    T->TestEqual(TEXT("Respawn creates a fresh creature at full health"),B->GetHealth(),B->GetMaxHealth()); T->TestEqual(TEXT("Respawning never pays XP"),M->Fighter(P)->XP-XP,180);
    int Corpses=0; for(TActorIterator<ACultCorpse> It(W);It;++It) {
     ++Corpses; const auto Bounds=It->GetComponentsBoundingBox(true); const float Clearance=Bounds.Min.Z-FCultMobaJungle::Ground(0).Z;
     T->AddInfo(FString::Printf(TEXT("Jungle corpse render-bounds ground clearance %.1f cm"),Clearance));
     T->TestTrue(TEXT("A scaled jungle corpse rests on its actual plateau surface"),Clearance>=-5&&Clearance<=10);
    } T->TestTrue(TEXT("A slain jungle creature leaves a persistent corpse"),Corpses>0);
    auto* Other=Camps[1]->Monster(); Place(M,Enemy,FCultMobaJungle::Ground(1)+FVector(-1600,0,0)); Enemy->GetCharacterMovement()->DisableMovement();
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Unit=W->SpawnActor<ACultPopulation>(FCultMobaJungle::Ground(1)+FVector(-1200,0,150),FRotator::ZeroRotator,Params); Unit->SetOwner(Enemy); Unit->SummonTo(1); Unit->SetActorTickEnabled(false); Summon=Unit;
    EnemyXP=M->Fighter(Enemy)->XP; XP=M->Fighter(P)->XP;
    Bolt(W,Unit,FCultMobaJungle::Ground(1)+FVector(-1200,0,220),Other); At=Now; Phase=9; return false;
   }
   if(Now-At>3) { T->AddError(TEXT("Camp failed to respawn")); return true; } return false;
  }
  if(Phase==9) {
   if(Camps[1]->RespawnRemaining()>0) {
    T->TestEqual(TEXT("A summoned fighter's physical kill credits its opposing-team owner"),M->Fighter(Enemy)->XP-EnemyXP,180);
    T->TestEqual(TEXT("The human player is not automatically rewarded for other teams' kills"),M->Fighter(P)->XP,XP);
    if(Summon.IsValid()) Summon->Destroy(); Park(M,P);
    Place(M,Enemy,FCultMobaJungle::Ground(2)+FVector(-1000,0,0)); Enemy->GetCharacterMovement()->DisableMovement(); Hp=Enemy->GetHealth(); At=Now; Phase=10; return false;
   }
   if(Now-At>3) { T->AddError(TEXT("Summon's physical projectile did not kill the opposite camp")); return true; } return false;
  }
  if(Phase==10) {
   if(auto* Attack=Windup(W,Camps[2]->Monster())) {
    T->TestEqual(TEXT("The warden retains a deliberately slower heavy windup"),Attack->Windup,.72f);
    T->TestEqual(TEXT("The warden targets the opposing bot hero on equal terms"),Camps[2]->Target(),Enemy);
    const auto Ground=FCultMobaJungle::Ground(2); auto* Camera=W->SpawnActor<ACameraActor>(Ground+FVector(-4500,-7000,7000),FRotator::ZeroRotator); Camera->SetActorRotation((Ground+FVector(0,0,600)-Camera->GetActorLocation()).Rotation()); W->GetFirstPlayerController()->SetViewTarget(Camera);
    FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/JungleWarden.png")),true,false);
    At=Now; Phase=11; return false;
   }
   if(Now-At>5) { T->AddError(TEXT("Warden failed to engage an opposing-team hero")); return true; } return false;
  }
  if(Phase==11) {
   if(Enemy->GetHealth()<Hp) {
    T->TestTrue(TEXT("The elite warden's real swipe deals its stronger damage"),Hp-Enemy->GetHealth()>=24&&Hp-Enemy->GetHealth()<=28.1f);
    auto* Warden=Camps[2]->Monster(); Hp=Warden->GetHealth();
    FCultJungleTestAccess::Retaliate(M,5); At=Now; Phase=12; return false;
   }
   if(Now-At>2) { T->AddError(TEXT("Warden swipe did not reach its in-range target")); return true; } return false;
  }
  if(Phase==13) {
   auto* Warden=Camps[2]->Monster();
   if(auto* Attack=Windup(W,Warden)) {
    TWeakObjectPtr<ACultSkillAttack> Pending(Attack);
    ACultSkillAttack::Hurt(Warden,60,P,Warden->GetActorLocation());
    T->TestTrue(TEXT("The heavy warden resists an ordinary first impact"),Warden->StaggerRemaining()==0&&Pending.IsValid());
    ACultSkillAttack::Hurt(Warden,75,P,Warden->GetActorLocation());
    T->TestTrue(TEXT("Sufficient actual damage can break the competitive warden's poise before death"),Warden->GetHealth()>0&&Warden->StaggerRemaining()>0&&!Pending.IsValid());
    Place(M,P,FCultMobaJungle::Ground(0)+FVector(-700,0,0)); Hp=P->GetHealth(); At=Now; Phase=2; return false;
   }
   if(Now-At>5) { T->AddError(TEXT("No warden windup for poise verification")); return true; } return false;
  }
  if(Phase==12) {
   auto* Warden=Camps[2]->Monster();
   if((!Warden||Warden->IsDead()||Warden->GetHealth()<Hp)&&Now-At>.25f) {
    T->TestTrue(TEXT("An actual bot decision retaliates against its neutral attacker with a physical kit attack"),true);
    Park(M,Enemy); const int Before=M->Fighter(Enemy)->XP;
    auto* Remote=Camps[3]->Monster(); ACultSkillAttack::Hurt(Remote,500,Enemy,Remote->GetActorLocation());
    T->TestEqual(TEXT("A lethal hit retains its source credit even after the owner has left the leash area"),M->Fighter(Enemy)->XP-Before,300);
    T->TestTrue(TEXT("The elite camp uses its longer 90-second respawn"),Camps[3]->RespawnRemaining()>85&&Camps[3]->RespawnRemaining()<=90);
    return true;
   }
   if(Now-At>2) { T->AddError(TEXT("Bot failed to physically retaliate against the jungle attacker")); return true; } return false;
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultJungleCampTest,"CultUlhu.Menu.JungleCamps",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultJungleCampTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FCultJungleCampObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
