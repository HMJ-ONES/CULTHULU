#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CultAutomationPIE.h"
#include "CultKitRuntime.h"
#include "CultMatchState.h"
#include "CultMovementTuning.h"
#include "CultPopulation.h"
#include "CultSkillAttack.h"
#include "CultUlhuGameInstance.h"
#include "CultWorldDirector.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/FileHelper.h"

struct FCultPlayabilityAccess {
 static FCultKitRuntime& Kit(ACultUlhuPlayerPawn* P) { return *P->Kit_; }
 static bool HasKit(ACultUlhuPlayerPawn* P) { return IsValid(P)&&P->Kit_; }
 static bool PowerSurge(ACultUlhuPlayerPawn* P) { return Kit(P).BuffTime>0&&Kit(P).BuffMult>1.f; }
 static void ApplyTelekineticImpact(ACultUlhuPlayerPawn* Source,ACultUlhuPlayerPawn* Target) {
  auto& Runtime=Kit(Source);
  Runtime.Snapshot();
  for(auto& Mirror:Runtime.Mirrors) if(Mirror.Actor.Get()==Target) Mirror.Core->takeDamage(1000000.f);
  Runtime.Apply();
 }
 static bool Control(ACultUlhuPlayerPawn* P,const std::string& Kind) {
  P->Control_.Tick(0);
  return Kind=="fear_aura"?P->Control_.Fear:Kind=="stun"?P->Control_.Stun:P->Control_.Speed<.99f;
 }
 static void PrepareMatch(ACultMatchDirector* Match) {
  Match->SetActorTickEnabled(false);
  Match->State_->Warmup=0;
  for(auto& Fighter:Match->State_->Fighters) {
   Fighter.Shield=0;
   if(Fighter.Pawn.IsValid()&&Fighter.Team==1) Fighter.Pawn->SetActorTickEnabled(false);
  }
  for(auto& Unit:Match->State_->Units) if(Unit.IsValid()) Unit->SetActorTickEnabled(false);
 }
 static void Register(ACultMatchDirector* Match,ACultUlhuPlayerPawn* Pawn,int32 Slot,const FString& Id) {
  auto& Fighter=Match->State_->Fighters[Slot];
  if(Fighter.Pawn.IsValid()&&Fighter.Pawn.Get()!=Pawn) Fighter.Pawn->Destroy();
  Fighter.Pawn=Pawn;
  Fighter.Kit=Id;
  Fighter.Level=1;
  Fighter.XP=0;
  Fighter.FirstPick=-1;
  Fighter.Shield=0;
  Fighter.Respawn=0;
 }
 static void LevelSix(ACultMatchDirector* Match) {
  Match->GrantXP(0,100);
  Match->GrantXP(0,150);
 }
 static void Stage(ACultUlhuPlayerPawn* P,const FBox& Bounds,FVector Feet) {
  P->TerrainBounds_=Bounds;
  P->RelocateMatch(Feet);
 }
 static void Aim(ACultUlhuPlayerPawn* P,FVector Point) {
  // Fixed shoulder shot isolates delivery. Movement runs with the normal camera.
  P->SpringArm_->SetComponentTickEnabled(false);
  if(P->Camera_->GetAttachParent()) P->Camera_->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
  const FVector Eye=P->GetActorLocation()+FVector(-1200,-220,80);
  P->Camera_->SetWorldLocationAndRotation(Eye,(Point-Eye).Rotation());
 }
 static ACultMatchUnit* Minion(ACultMatchDirector* M,FVector Feet) {
  auto* Unit=M->AddUnit(1,1,0,-1,Feet,100,0,0,0);
  Unit->SetActorTickEnabled(false);
  return Unit;
 }
};

namespace {
const FVector Arena(200000,0,60000);
enum class EPhase { Spawn, Walk, Sprint, Dash, Action, Observe };

class FPlayabilityMatrix final : public IAutomationLatentCommand {
 FAutomationTestBase& Test;
 const ECultPlayMode Mode;
 const double Started=FPlatformTime::Seconds();
 bool Entered=false;
 int32 Index=0,Action=0;
 EPhase Phase=EPhase::Spawn;
 float PhaseAt=0,WalkVelocity=0,SprintVelocity=0,StaminaBefore=0,Before=0,PrimaryDamage=0;
 int32 Casts=0,Hits=0;
 bool Heavy=false;
 FVector Start,EnemyFeet,BeforePosition;
 FString Label,Report=TEXT("character,walk_cm_s,sprint_cm_s,primary_damage_pct,rmb_committed,qfr_committed,qfr_observed_effects\n");
 TWeakObjectPtr<ACultUlhuPlayerPawn> Player,Enemy;
 TWeakObjectPtr<ACultMatchUnit> SlingMinion;
 FBox Bounds{ForceInit};
 FVector OriginalFeet;

 static ACultUlhuPlayerPawn* Spawn(UWorld* World,const FString& Id,FVector Feet,int32 Team) {
  const FTransform Transform(Feet+FVector(0,0,1500));
  auto* Pawn=World->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(!Pawn) return nullptr;
  Pawn->ConfigurePlayer(Id,Team);
  Pawn->FinishSpawning(Transform);
  return Pawn;
 }
 void ClearDelivery(UWorld* World,ACultUlhuPlayerPawn* Owner) {
  for(TActorIterator<ACultSkillAttack> It(World);It;++It) if(It->GetOwner()==Owner) It->Destroy();
  for(TActorIterator<ACultPopulation> It(World);It;++It) if(It->GetOwner()==Owner) It->Destroy();
  if(SlingMinion.IsValid()) SlingMinion->Destroy();
  SlingMinion.Reset();
 }
 void Check(const TCHAR* Message,bool Passed) { Test.TestTrue(Label+TEXT(" ")+Message,Passed); }

public:
 FPlayabilityMatrix(FAutomationTestBase& InTest,ECultPlayMode InMode):Test(InTest),Mode(InMode) {}

 bool Update() override {
  using Access=FCultPlayabilityAccess;
  using Move=CultAttackMotion::EMove;
  if(FPlatformTime::Seconds()-Started>480) { Test.AddError(Label+TEXT(" matrix timed out")); return true; }
  auto* World=GEditor->PlayWorld.Get();
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  if(!PC) return false;
  auto* Game=Cast<UCultUlhuGameInstance>(World->GetGameInstance());
  const auto& Roster=Game->Roster();
  if(!Entered) {
   if(!World->GetName().Contains(TEXT("L_MainMenu"))) return false;
   Game->OpenCharacterSelect(Mode);
   Game->SelectedCharacterId=Roster[0].Id;
   if(Mode==ECultPlayMode::FreeRoam) {
    Game->FreeRoamSaveSlot=TEXT("Automation_Playability");
    Game->SetInitialBeliefs({5,6,11});
   }
   Game->StartPlay();
   Entered=true;
   return false;
  }
  if(!World->GetName().Contains(TEXT("L_DarkWilds"))||World->GetTimeSeconds()<2.f) return false;
  const auto* Director=ACultWorldDirector::Find(World);
  if(!Director) return false;
  auto* Match=ACultMatchDirector::Find(World);
  if(Mode!=ECultPlayMode::FreeRoam&&!Match) return false;
  const float Now=World->GetTimeSeconds();
  auto* P=Player.Get();
  auto* E=Enemy.Get();

  if(Phase==EPhase::Spawn) {
   const FString& Id=Roster[Index].Id;
   Label=UCultUlhuGameInstance::ModeDisplayName(Mode)+TEXT(" / ")+Id;
   if(!Bounds.IsValid) {
    P=Cast<ACultUlhuPlayerPawn>(PC->GetPawn());
    if(!Access::HasKit(P)) return false;
    OriginalFeet=P->FeetLocation();
    if(Match) Access::PrepareMatch(Match);
    auto* Floor=World->SpawnActor<AStaticMeshActor>(Arena-FVector(0,0,50),FRotator::ZeroRotator);
    auto* Mesh=Floor->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Mesh->SetWorldScale3D(FVector(280,230,1));
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Bounds=Mesh->Bounds.GetBox();
   } else {
    if(!IsValid(P)) { Test.AddError(Label+TEXT(" lost the previous player pawn")); return true; }
    ClearDelivery(World,P);
    PC->UnPossess();
    P->Destroy();
    if(IsValid(E)) E->Destroy();
    Game->SelectedCharacterId=Id;
    P=Spawn(World,Id,OriginalFeet,0);
    if(!Access::HasKit(P)) { Test.AddError(Label+TEXT(" failed to create a playable pawn")); return true; }
    PC->Possess(P);
   }
   Player=P;
   Check(TEXT("spawns upright with walking enabled"),P->GetCharacterMovement()->IsMovingOnGround()&&P->GetActorUpVector().Z>.98);
   Check(TEXT("has the selected native kit"),Access::Kit(P).Def.id==TCHAR_TO_UTF8(*Id));
   P->AltReleased();
   if(Match) Access::Register(Match,P,0,Id);
   if(Mode==ECultPlayMode::Dotgoo) {
    Check(TEXT("ultimate is locked at level one"),!Match->AbilityAvailable(P,2));
    Access::LevelSix(Match);
    Check(TEXT("the normal level progression unlocks the full kit"),Match->AbilityAvailable(P,0)&&Match->AbilityAvailable(P,1)&&Match->AbilityAvailable(P,2));
   }
   Access::Stage(P,Bounds,Arena);
   PC->SetControlRotation(FRotator::ZeroRotator);
   P->SetEnhancedMove(FVector2D(0,1));
   Start=P->FeetLocation();
   Phase=EPhase::Walk;
   PhaseAt=Now;
   return false;
  }
  if(!Access::HasKit(P)) { Test.AddError(Label+TEXT(" lost the player pawn")); return true; }
  auto& Kit=Access::Kit(P);
  const float OldWalk=Kit.Def.moveSpeed*100.f*FMath::Sqrt(P->GetActorScale3D().X)*1.25f;
  if(Phase==EPhase::Walk&&Now-PhaseAt>.65f) {
   WalkVelocity=P->GetVelocity().Size2D();
   const float Target=OldWalk*CultMovement::WalkIncrease;
   // Sample a settled stride, allowing time for a first-use mesh/shader hitch.
   if(!FMath::IsNearlyEqual(WalkVelocity,Target,20.f)&&Now-PhaseAt<2.4f) return false;
   Check(*FString::Printf(TEXT("movement reaches the faster walk speed (%.0f / %.0f cm/s)"),WalkVelocity,Target),
    FMath::IsNearlyEqual(WalkVelocity,Target,20.f)&&P->FeetLocation().X>Start.X+300);
   Check(TEXT("walking leaves stamina full"),FMath::IsNearlyEqual(Kit.StaminaValue(),Kit.Def.maxStamina,.01f));
   P->SetEnhancedSprint(true);
   Phase=EPhase::Sprint;
   PhaseAt=Now;
  } else if(Phase==EPhase::Sprint&&Now-PhaseAt>.76f) {
   SprintVelocity=P->GetVelocity().Size2D();
   const float Target=OldWalk*1.85f*CultMovement::SprintIncrease;
   if(!FMath::IsNearlyEqual(SprintVelocity,Target,35.f)&&Now-PhaseAt<2.4f) return false;
   Check(*FString::Printf(TEXT("hold to sprint reaches the tuned speed (%.0f / %.0f cm/s)"),SprintVelocity,Target),
    FMath::IsNearlyEqual(SprintVelocity,Target,35.f));
   Check(TEXT("hold drains stamina without dashing"),Kit.StaminaValue()<Kit.Def.maxStamina&&!P->IsDashing());
   P->SetEnhancedSprint(false);
   P->SetEnhancedMove(FVector2D::ZeroVector);
   const float BeforeDash=Kit.StaminaValue();
   Check(TEXT("a standard dash spends one third of the stamina gauge"),P->TryDash(FVector::RightVector)&&FMath::IsNearlyEqual(Kit.StaminaValue(),BeforeDash-Kit.Def.maxStamina/3.f,.01f));
   Check(TEXT("standard dash keeps its reach and mode cooldown"),FMath::IsNearlyEqual(P->DashTravelDistance(),OldWalk*3.5f*.22f,1.f)&&
    FMath::IsNearlyEqual(P->CombatHUD().DashCooldown,Mode==ECultPlayMode::Dotgoo?1.5f:1.15f,.01f));
   Start=P->FeetLocation();
   Phase=EPhase::Dash;
   PhaseAt=Now;
  } else if(Phase==EPhase::Dash&&Now-PhaseAt>.48f) {
   Check(TEXT("dash actually carries the vessel sideways"),P->FeetLocation().Y>Start.Y+200);
   Access::Stage(P,Bounds,Arena);
   E=Spawn(World,Roster[Index].Id,OriginalFeet+FVector(0,7500,0),1);
   if(!Access::HasKit(E)) { Test.AddError(Label+TEXT(" failed to spawn the opponent fixture")); return true; }
   Enemy=E;
   if(Match) Access::Register(Match,E,5,Roster[Index].Id);
   E->SetActorTickEnabled(false);
   E->GetCharacterMovement()->DisableMovement();
   EnemyFeet=Arena+FVector(P->GetSimpleCollisionRadius()*2.f+330.f,0,0);
   Action=0; Casts=0; Hits=0; Heavy=false; PrimaryDamage=0;
   Phase=EPhase::Action;
  } else if(Phase==EPhase::Action) {
   ClearDelivery(World,P);
   Access::Stage(P,Bounds,Arena);
   const cultulhu::SpellDef* Spells[]={&Kit.Def.qAbility,&Kit.Def.fAbility,&Kit.Def.rAbility};
   const auto* Spell=Action>=2?Spells[Action-2]:nullptr;
   const bool Cthulhu=Kit.Def.id=="cthulhu_avatar";
   const bool Healing=Spell&&Spell->effectKind=="heal";
   const bool Summon=Spell&&Spell->effectKind=="summon";
   E->RespawnMatch(EnemyFeet);
   E->GetCharacterMovement()->DisableMovement();
   if(Action==1&&Cthulhu&&Mode==ECultPlayMode::Domination) {
    const float Health=E->GetHealth();
    Access::ApplyTelekineticImpact(P,E);
    Check(TEXT("lethal core impact damage is capped against a competitive hero"),FMath::IsNearlyEqual(Health-E->GetHealth(),18.f,.1f));
    E->RespawnMatch(EnemyFeet);
    E->GetCharacterMovement()->DisableMovement();
   }
   FVector Aim=E->GetActorLocation();
   if(Healing||(Summon&&Mode!=ECultPlayMode::Domination)||(Action==1&&Cthulhu&&Mode!=ECultPlayMode::Dotgoo))
    Aim=Arena+FVector(600,2200,0);
   if(Action==1&&Cthulhu&&Mode==ECultPlayMode::Dotgoo) {
    SlingMinion=Access::Minion(Match,Arena+FVector(1700,1900,0));
    Aim=SlingMinion->GetActorLocation();
   }
   Access::Aim(P,Aim);
   // Alternate drained and full-gauge Q/F/R to catch both gating and deductions.
   if(Action!=3) Kit.SpendStamina(Kit.StaminaValue());
   else Kit.Sprint(false,20.f);
   StaminaBefore=Kit.StaminaValue();
   Before=Healing?Kit.Damage(P->GetMaxHealth()*.25f):E->GetHealth();
   BeforePosition=E->GetActorLocation();
   const ECultPlayerInput Keys[]={ECultPlayerInput::Primary,ECultPlayerInput::Heavy,ECultPlayerInput::Q,ECultPlayerInput::F,ECultPlayerInput::R};
   const auto WardCapacity=[&] {
    return Mode==ECultPlayMode::FreeRoam
     ? Spell->effectPower*Director->CombatMultiplier(P->GetFaction())
     : CultMatchBalance::CompetitiveBaseHealth*.18f;
   };
   const float ExpectedWard=Spell&&Spell->effectKind=="shield"?WardCapacity():0.f;
   if(Spell) Kit.SetCooldown(Action-2,0);
   P->RoutePlayerInput(Keys[Action],true);
   if(Spell) Check(TEXT("Holding an ability does not start its cooldown"),Kit.Cooldown(Action-2)==0);
   if(Spell||(Action==1&&!Cthulhu)) P->RoutePlayerInput(Keys[Action],false);
   Check(TEXT("action leaves the stamina gauge untouched"),FMath::IsNearlyEqual(Kit.StaminaValue(),StaminaBefore,.01f));
   if(Spell) {
    const float Cooldown=Kit.Cooldown(Action-2);
    const bool Committed=Cooldown>0;
    Check(*FString::Printf(TEXT("%s commits with legal aim and uses its cooldown"),UTF8_TO_TCHAR(Spell->id.c_str())),Committed);
    Casts+=Committed;
    Check(TEXT("ability begins its own cast animation"),P->AttackPlayback().Move==CultAttackMotion::AbilitySlot(Action-2));
    P->RoutePlayerInput(Keys[Action],true);
    P->RoutePlayerInput(Keys[Action],false);
    Check(TEXT("a second input cannot reset the active cooldown"),Kit.Cooldown(Action-2)==Cooldown);
    if(Spell->effectKind=="shield") {
     Check(TEXT("ward is limited to one cast's capacity"),FMath::IsNearlyEqual(Kit.Ward(),ExpectedWard,.1f));
     Kit.SetCooldown(Action-2,0);
     P->RoutePlayerInput(Keys[Action],false);
     const float ExpectedRecast=WardCapacity();
     P->RoutePlayerInput(Keys[Action],true);
     P->RoutePlayerInput(Keys[Action],false);
     Check(TEXT("recasting cannot accumulate a second ward"),FMath::IsNearlyEqual(Kit.Ward(),ExpectedRecast,.1f));
    }
   } else if(Action==0) {
    Check(TEXT("LMB commits its primary choreography at zero stamina"),P->AttackPlayback().Move==(Kit.Primary.Ranged()?Move::Primary:Move::Melee));
    Check(TEXT("the aimed primary has a windup"),E->GetHealth()==Before);
   } else {
    Heavy=P->AttackPlayback().Move==Move::Heavy||P->AttackPlayback().Move==Move::Wave||
     P->AttackPlayback().Move==Move::RockLift||P->AttackPlayback().Move==Move::MinionLift;
    Check(TEXT("RMB commits its alternate action at zero stamina"),Heavy);
   }
   P->RoutePlayerInput(Keys[Action],false);
   Phase=EPhase::Observe;
   PhaseAt=Now;
  } else if(Phase==EPhase::Observe&&Now-PhaseAt>1.22f) {
   if(Action==0) {
    PrimaryDamage=(Before-E->GetHealth())/E->GetMaxHealth()*100.f;
    Check(TEXT("a centered primary actually hits the stationary hero"),PrimaryDamage>0);
    Check(TEXT("the opening primary is below 25% of equal-health hero HP"),PrimaryDamage<25.f);
   } else if(Action>=2) {
    const cultulhu::SpellDef* Spells[]={&Kit.Def.qAbility,&Kit.Def.fAbility,&Kit.Def.rAbility};
    const auto& Spell=*Spells[Action-2];
    const auto& Kind=Spell.effectKind;
    bool Observed=E->GetHealth()<Before;
    if(Kind=="heal") Observed=P->GetHealth()>Before;
    if(Kind=="dash") Observed=FVector::Dist2D(P->FeetLocation(),Arena)>100.f;
    if(Kind=="stun"||Kind=="fear_aura"||Kind=="debuff") Observed=Access::Control(E,Kind);
    if(Kind=="pull") Observed=FVector::Dist2D(E->GetActorLocation(),BeforePosition)>100.f;
    if(Kind=="buff") Observed=Access::PowerSurge(P);
    if(Kind=="shield") {
     const float Health=P->GetHealth();
     P->ReceiveDamage(10.f,E);
     Observed=P->GetHealth()==Health;
     if(Mode!=ECultPlayMode::FreeRoam) {
      Kit.Tick(4.f);
      Check(TEXT("the competitive ward expires after four seconds"),Kit.Ward()==0);
     }
    }
    if(Kind=="summon"&&Mode!=ECultPlayMode::Domination)
     for(TActorIterator<ACultPopulation> It(World);It;++It) Observed|=It->GetOwner()==P&&It->IsSummoned();
    Hits+=Observed;
    Check(*FString::Printf(TEXT("%s produces its expected effect"),UTF8_TO_TCHAR(Spell.id.c_str())),Observed);
   }
   if(++Action<5) Phase=EPhase::Action;
   else {
    Report+=FString::Printf(TEXT("%s,%.0f,%.0f,%.2f,%d,%d,%d\n"),*Roster[Index].Id,WalkVelocity,SprintVelocity,PrimaryDamage,Heavy,Casts,Hits);
    if(++Index<Roster.Num()) { Phase=EPhase::Spawn; return false; }
    ClearDelivery(World,P);
    const FString File=FPaths::ProjectSavedDir()/FString::Printf(TEXT("Playability_Mode%d.csv"),int32(Mode));
    Test.TestTrue(TEXT("Write the complete measured roster"),FFileHelper::SaveStringToFile(Report,*File));
    return true;
   }
  }
  return false;
 }
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCultPlayabilityMatrix,"CultUlhu.Playability.Matrix",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FCultPlayabilityMatrix::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const {
 Names={TEXT("FreeRoam"),TEXT("Domination"),TEXT("DOTGOO")};
 Commands={TEXT("0"),TEXT("1"),TEXT("2")};
}
bool FCultPlayabilityMatrix::RunTest(const FString& Parameter) {
 ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE());
 ADD_LATENT_AUTOMATION_COMMAND(FPlayabilityMatrix(*this,ECultPlayMode(FCString::Atoi(*Parameter))));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
