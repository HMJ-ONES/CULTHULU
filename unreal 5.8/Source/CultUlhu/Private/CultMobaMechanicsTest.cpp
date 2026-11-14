#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "CultAutomationPIE.h"
#include "CultMatchState.h"
#include "CultKitRuntime.h"
#include "CultSkillAttack.h"
#include "CultHumanMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/PoseableMeshComponent.h"
#include "CultUlhuGameInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

struct FCultMobaMechanicsTestAccess {
 static void Isolate(ACultMatchDirector* M,ACultUlhuPlayerPawn* P) {
  int I=0; for(auto& F:M->State_->Fighters) { F.Shield=0; if(auto* H=F.Pawn.Get()) {
   H->bBotControlled_=false; H->EvadeTime_=0; H->GetCharacterMovement()->DisableMovement();
   H->RelocateMatch(FVector(-32000+I++*2200,-18000,0));
  } }
  for(auto& U:M->State_->Units) if(U.IsValid()) { U->Range=0; U->Dps=0; if(!U->IsStructure()) { U->Speed=0; U->SetActorLocation(FVector(-30000,-17000,153)); } }
  P->RelocateMatch(FVector(0,18000,0));
  P->SpringArm_->SetComponentTickEnabled(false); P->Camera_->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
 }
 static void Aim(ACultUlhuPlayerPawn* P,FVector Target) {
  const FVector Start=P->FeetLocation()+FVector(0,0,700); P->Camera_->SetWorldLocationAndRotation(Start,(Target-Start).Rotation());
 }
 static void Ready(ACultUlhuPlayerPawn* P) { P->Kit_->HeavyCooldown=0; }
 static float Cooldown(ACultUlhuPlayerPawn* P) { return P->Kit_->HeavyCooldown; }
 static void SubstituteOpponent(ACultMatchDirector* Match,ACultUlhuPlayerPawn* From,ACultUlhuPlayerPawn* To) {
  for(auto& Fighter:Match->State_->Fighters) if(Fighter.Pawn.Get()==From) { Fighter.Pawn=To; return; }
 }
 static ACultMatchUnit* Spawn(ACultMatchDirector* M,int Team,FVector At) { return M->AddUnit(Team,2,0,-1,At,120,0,0,600); }
 static void Shift(FAutomationTestBase* T,ACultUlhuPlayerPawn* P) {
  auto Reset=[&] { P->GetCharacterMovement()->RemoveRootMotionSource(TEXT("CultDash")); P->DashTime_=0; P->DashCooldown_=0; P->Kit_->Stamina.value=P->Kit_->Def.maxStamina; };
  Reset(); bool Sprint=false;
  for(int I=0;I<40;++I) Sprint=P->ProcessShift(true,I%2?FVector::ForwardVector:FVector::RightVector,.02f);
  T->TestTrue(TEXT("Holding Shift enters sprint"),Sprint); T->TestEqual(TEXT("Holding and changing direction never starts a dash"),P->DashTime_,0.f);
  P->ProcessShift(false,FVector::ForwardVector,.02f); T->TestEqual(TEXT("Releasing a long hold never dashes"),P->DashTime_,0.f);
  for(FVector D:{FVector::ForwardVector,-FVector::ForwardVector,FVector::RightVector,-FVector::RightVector}) {
   Reset(); T->TestFalse(TEXT("Initial Shift press waits to distinguish a hold"),P->ProcessShift(true,D,.04f));
   T->TestEqual(TEXT("Dash is not triggered on press"),P->DashTime_,0.f); P->ProcessShift(false,D,.02f);
   T->TestTrue(TEXT("A short tap launches its selected direction"),P->DashTime_>0&&P->DashDirection_.Equals(D,.01));
   P->DashTime_=0; P->ProcessShift(false,D,.02f); T->TestEqual(TEXT("A tap cannot repeat while Shift stays up"),P->DashTime_,0.f);
  }
  Reset(); P->ProcessShift(true,FVector::ForwardVector,.04f); P->ProcessShift(true,FVector::ZeroVector,.02f,false); P->ProcessShift(false,FVector::ForwardVector,.02f);
  T->TestEqual(TEXT("Cursor/control cancellation cannot queue a dash"),P->DashTime_,0.f); Reset();
  for(int Index=0;Index<3;++Index) {
   P->DashCooldown_=0;
   T->TestTrue(TEXT("Three dashes are available from full stamina"),P->TryDash(FVector::RightVector));
   const float Remaining=P->Kit_->StaminaValue();
   P->Kit_->Sprint(false,1.5f);
   T->TestEqual(TEXT("Stamina does not regenerate before the next dash opens"),P->Kit_->StaminaValue(),Remaining);
  }
  P->DashCooldown_=0;
  T->TestFalse(TEXT("A fourth dash is unavailable on an empty gauge"),P->CanDash());
  P->Kit_->Sprint(false,.50f);
  T->TestTrue(TEXT("Stamina recovers after the delay expires"),P->Kit_->StaminaValue()>0);
  Reset();
  P->UpdateFlight(true,false,.25f);
  const float Normal=P->Kit_->StaminaValue(), FlightSpeed=P->GetCharacterMovement()->MaxFlySpeed;
  T->TestTrue(TEXT("Cthulhu takes flight while holding Space"),P->GetCharacterMovement()->IsFlying()&&P->GetVelocity().Z>0);
  P->UpdateFlight(true,true,.25f);
  T->TestTrue(TEXT("Boost flight trades more stamina for speed"),Normal-P->Kit_->StaminaValue()>P->Kit_->Def.maxStamina-Normal&&P->GetCharacterMovement()->MaxFlySpeed>FlightSpeed);
  P->UpdateFlight(false,false,.01f);
  T->TestTrue(TEXT("Releasing flight returns the character to falling"),P->GetCharacterMovement()->IsFalling());
  P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  P->GetCharacterMovement()->StopMovementImmediately();
  Reset();
  const int Team=P->GetFaction();
  const auto Ally=P->TeamColorFor(Team), Foe=P->TeamColorFor(1-Team);
  T->TestTrue(TEXT("Allies are blue and opponents are red from the local perspective"),Ally.B>Ally.R&&Foe.R>Foe.B);
  for(TActorIterator<ACultUlhuPlayerPawn> It(P->GetWorld());It;++It) if(It->GetCharacterId()==TEXT("yig")) {
   T->TestTrue(TEXT("Yig uses an elevated combat collider separate from his movement capsule"),
    It->CombatCapsule()!=It->GetCapsuleComponent()&&
    It->CombatCapsule()->Bounds.Origin.Z-It->CombatCapsule()->Bounds.BoxExtent.Z>It->FeetLocation().Z+
    It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*.6f);
   break;
  }
 }
 static void VerifyWardShots(FAutomationTestBase* T,ACultMatchDirector* M,ACultUlhuPlayerPawn* P,ACultMatchUnit* Tower) {
  const FVector Origin=Tower->GetActorLocation(); const FVector Feet=Origin+FVector(3000,0,-1253);
  const float SavedRange=Tower->Range,SavedDps=Tower->Dps,SavedCooldown=Tower->AttackCooldown; Tower->Range=4000; Tower->Dps=20;
  auto Fire=[&]() { Tower->AttackCooldown=0; Tower->FireAt(P); ACultSkillAttack* Shot=nullptr; for(TActorIterator<ACultSkillAttack> It(M->GetWorld());It;++It) if(It->GetOwner()==Tower) Shot=*It; if(Shot) Shot->Tick(0); return Shot; };
  auto Clear=[&]() { for(TActorIterator<ACultSkillAttack> It(M->GetWorld());It;++It) if(It->GetOwner()==Tower) It->Destroy(); };
  Clear(); P->RelocateMatch(Feet); P->EvadeTime_=P->DashTime_=P->DashCooldown_=0;
  float Before=P->GetHealth(); auto* Shot=Fire(); T->TestNotNull(TEXT("Actual tower creates a ward shot"),Shot);
  if(Shot) {
   for(int I=0;I<35&&!Shot->IsActorBeingDestroyed();++I) { const float A=I*.012f; P->RelocateMatch(Origin+FVector(FMath::Cos(A)*3000,FMath::Sin(A)*3000,-1253)); Shot->Tick(.02f); }
   T->TestTrue(TEXT("Circling an actual tower cannot avoid its locked shot"),P->GetHealth()<Before);
  }
  Clear(); P->RelocateMatch(Feet); P->EvadeTime_=.14f; Before=P->GetHealth();
  P->ReceiveDamage(20,Tower); T->TestTrue(TEXT("Jump evade time does not reject tower damage"),P->GetHealth()<Before);
  P->EvadeTime_=0; Shot=Fire(); Before=P->GetHealth();
  if(Shot) {
   Shot->Tick(.26f); T->TestTrue(TEXT("A real dash is available against an incoming tower shot"),P->TryDash(FVector::RightVector));
   T->TestEqual(TEXT("DOTGOO dash uses the revised cooldown"),P->CombatHUD().DashCooldown,1.5f);
   Shot->Tick(.01f); T->TestTrue(TEXT("Dash permanently breaks the in-flight ward lock"),Shot->IsActorBeingDestroyed());
   T->TestEqual(TEXT("A dashed tower shot deals no damage"),P->GetHealth(),Before);
   T->TestFalse(TEXT("Dash cannot be immediately repeated in DOTGOO"),P->CanDash());
  }
  Clear(); P->GetCharacterMovement()->RemoveRootMotionSource(TEXT("CultDash")); P->DashTime_=P->DashCooldown_=P->EvadeTime_=0;
  Tower->Range=SavedRange; Tower->Dps=SavedDps; Tower->AttackCooldown=SavedCooldown;
 }
 static AActor* Target(ACultMatchDirector* M,ACultMatchUnit* Tower) { return M->SelectTowerTarget(Tower); }
 static void Cancel(ACultUlhuPlayerPawn* P) { P->Control_.Set(991,true,false,1,FVector::ZeroVector,true); P->Control_.Tick(.001f); P->Kit_->TickMinionThrow(.1f); P->Control_.Sources.Remove(991); P->Control_.Tick(.001f); }
};

class FCultMobaMechanicsObservation:public IAutomationLatentCommand {
 FAutomationTestBase* T; int Phase=0; double Began=FPlatformTime::Seconds(); float At=0,Before=0; FVector Pickup,Land;
 TWeakObjectPtr<ACultMatchUnit> Minion,Tower;
 TWeakObjectPtr<ACultUlhuPlayerPawn> Victim,AcidCaster;
 TWeakObjectPtr<ACultSkillAttack> TestRock;
 float DirectHealth=0;
public:
 explicit FCultMobaMechanicsObservation(FAutomationTestBase* Test):T(Test) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>100) { T->AddError(TEXT("MOBA mechanics timed out")); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(Phase==0) { auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->SelectedCharacterId=TEXT("cthulhu_avatar"); GI->OpenCharacterSelect(ECultPlayMode::Dotgoo); GI->OpenBeliefSelect(); Phase=1; } return false; }
  auto* M=ACultMatchDirector::Find(W); auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0)); if(!M||!P||M->Warmup()>0) return false;
  if(Phase==1) {
   // Warmup reaches zero before the next director tick deploys its first wave.
   // Observe real spawned lane units rather than passing an empty-art check.
   bool WaveReady=false; for(TActorIterator<ACultMatchUnit> It(W);It;++It) WaveReady|=!It->IsStructure();
   if(!WaveReady) return false;
   int32 AuthoredMinions=0;
   for(TActorIterator<ACultMatchUnit> It(W);It;++It) {
    if(!It->IsStructure()) {
     TArray<UPoseableMeshComponent*> Body; It->GetComponents(Body);
     T->TestEqual(TEXT("Lane minion retains its complete art in one skinned component"),Body.Num(),1);
     if(Body.Num()==1) T->TestEqual(TEXT("Lane minion uses all six certified original source pieces"),CultHumanMesh::SourceParts(Cast<USkeletalMesh>(Body[0]->GetSkinnedAsset())),6);
     ++AuthoredMinions;
    }
    if(It->Kind==3) T->TestEqual(TEXT("Lane wards use the revised health budget"),It->MaxHealth,CultMatchBalance::LaneTowerHealth);
    if(It->Kind==5) T->TestEqual(TEXT("Nexus wards have 4000 HP"),It->MaxHealth,4000.f);
    if(It->Team==1&&It->Kind==3&&!M->UnitProtected(*It)) Tower=*It;
   }
   T->TestTrue(TEXT("Merged-body check covers actual spawned lane minions"),AuthoredMinions>0);
   FCultMobaMechanicsTestAccess::Isolate(M,P); FCultMobaMechanicsTestAccess::Shift(T,P);
   T->TestTrue(TEXT("Cthulhu MOBA uses the explicitly requested RMB variant"),P->CombatHUD().RmbHint.Contains(TEXT("minion")));
   if(!Tower.IsValid()) { T->AddError(TEXT("No exposed outer tower")); return true; }
   Victim=M->Fighters()[5].Pawn; Tower->SetActorLocation(FVector(0,18000,1253)); Tower->Range=3500; Tower->Dps=20; Tower->AttackCooldown=0;
   FCultMobaMechanicsTestAccess::VerifyWardShots(T,M,P,Tower.Get());
   P->RelocateMatch(FVector(2600,18000,0)); auto* Ally=M->Fighters()[1].Pawn.Get(); Ally->RelocateMatch(FVector(3300,18000,0));
   T->TestEqual(TEXT("Tower acquires the closest hostile hero"),FCultMobaMechanicsTestAccess::Target(M,Tower.Get()),static_cast<AActor*>(P));
   Ally->RelocateMatch(FVector(-1800,18000,0)); T->TestEqual(TEXT("Closer second hero cannot steal existing aggression"),FCultMobaMechanicsTestAccess::Target(M,Tower.Get()),static_cast<AActor*>(P));
   auto* Wave=FCultMobaMechanicsTestAccess::Spawn(M,0,FVector(2000,18000,0)); Wave->Speed=0;
   T->TestEqual(TEXT("Friendly lane wave entering area takes tower aggression"),FCultMobaMechanicsTestAccess::Target(M,Tower.Get()),static_cast<AActor*>(Wave));
   Wave->SetActorLocation(FVector(-30000,-17000,153)); T->TestTrue(TEXT("Tower resumes hero fire when the covering wave leaves"),Cast<ACultUlhuPlayerPawn>(FCultMobaMechanicsTestAccess::Target(M,Tower.Get()))!=nullptr);
   Ally->RelocateMatch(FVector(-32000,-18000,0)); Tower->AttackTarget=P; Before=P->GetHealth(); Tower->FireAt(P);
   bool Fast=false; for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==Tower.Get()) Fast=It->Speed==20250;
   T->TestTrue(TEXT("Actual tower projectile travels at 20250 cm/s"),Fast); T->TestEqual(TEXT("Tower shot has visible windup before contact"),P->GetHealth(),Before);
   Tower->AttackCooldown=100; At=W->GetTimeSeconds(); Phase=2; return false;
  }
  if(Phase==2) {
   if(W->GetTimeSeconds()-At<.8f) return false;
   T->TestTrue(TEXT("Fast tower projectile physically damages its aimed hero"),P->GetHealth()<Before);
   Tower->Range=0; Tower->Dps=0; Tower->SetActorLocation(FVector(18000,-18000,1253)); P->RelocateMatch(FVector(0,18000,0));
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) It->Destroy();
   Minion=FCultMobaMechanicsTestAccess::Spawn(M,1,FVector(2500,18000,0)); Pickup=Minion->GetActorLocation(); Minion->Speed=0;
   FCultMobaMechanicsTestAccess::Aim(P,Minion->GetActorLocation()); P->HeavyAttack();
   T->TestTrue(TEXT("Actual RMB camera aim lifts a hostile minion"),Minion->IsHeldBy(P)); T->TestEqual(TEXT("Successful pickup starts the 20 second cooldown"),FCultMobaMechanicsTestAccess::Cooldown(P),20.f);
   T->TestEqual(TEXT("Pickup does not damage its minion"),Minion->Health,120.f); At=W->GetTimeSeconds(); Phase=3; return false;
  }
  if(Phase==3) {
   if(W->GetTimeSeconds()-At<.45f) return false;
   T->TestTrue(TEXT("Held minion visibly levitates above the arena"),Minion->GetActorLocation().Z>600);
   FCultMobaMechanicsTestAccess::Aim(P,Minion->GetActorLocation()); At=W->GetTimeSeconds(); Phase=31; return false;
  }
  if(Phase==31) {
   if(W->GetTimeSeconds()-At<.35f) return false;
   if(FApp::CanEverRender()) FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/MobaMinionHeld.png")),true,false);
   Phase=30; return false;
  }
  if(Phase==30) {
   Victim->RelocateMatch(FVector(5500,18000,0)); Before=Victim->GetHealth(); FCultMobaMechanicsTestAccess::Aim(P,Victim->GetActorLocation()); P->ReleaseHeavy();
   T->TestTrue(TEXT("Release launches the held minion"),Minion->IsInTransit()&&!Minion->IsHeldBy(P)); T->TestEqual(TEXT("Throw is not an instant hit"),Victim->GetHealth(),Before);
   At=W->GetTimeSeconds(); Phase=4; return false;
  }
  if(Phase==4) {
   if(W->GetTimeSeconds()-At<1.5f) return false;
   T->AddInfo(FString::Printf(TEXT("Thrown hero impact %.1f -> %.1f HP, minion %.1f"),Before,Victim->GetHealth(),Minion.IsValid()?Minion->Health:-1));
   T->TestTrue(TEXT("Physical minion projectile damages an enemy hero"),Victim->GetHealth()<Before-10); T->TestTrue(TEXT("Successful hero impact kills the thrown minion"),!Minion.IsValid()||Minion->Health<=0);
   Victim->RelocateMatch(FVector(-18000,-18000,0));
   Minion=FCultMobaMechanicsTestAccess::Spawn(M,1,FVector(2500,18000,0)); Minion->Speed=0; FCultMobaMechanicsTestAccess::Aim(P,Minion->GetActorLocation()); P->HeavyAttack();
   T->TestFalse(TEXT("Another pickup is refused during real cooldown"),Minion->IsInTransit()); FCultMobaMechanicsTestAccess::Ready(P); P->HeavyAttack();
   Tower->SetActorLocation(FVector(5500,18000,1253)); Before=Tower->Health; At=W->GetTimeSeconds(); Phase=5; return false;
  }
  if(Phase==5) { if(W->GetTimeSeconds()-At<.4f) return false; FCultMobaMechanicsTestAccess::Aim(P,Tower->GetActorLocation()); P->Smash(); At=W->GetTimeSeconds(); Phase=6; return false; }
  if(Phase==6) {
   if(W->GetTimeSeconds()-At<1.5f) return false;
   T->TestTrue(TEXT("LMB launches the held minion into a real enemy tower"),Tower->Health<Before-20); T->TestTrue(TEXT("Tower impact consumes the thrown minion"),!Minion.IsValid()||Minion->Health<=0);
   Tower->SetActorLocation(FVector(18000,-18000,1253)); P->ReleaseHeavy(); FCultMobaMechanicsTestAccess::Ready(P);
   Minion=FCultMobaMechanicsTestAccess::Spawn(M,1,FVector(2500,18000,0)); Minion->Speed=0; Minion->Health=80; Pickup=Minion->GetActorLocation();
   FCultMobaMechanicsTestAccess::Aim(P,Minion->GetActorLocation()); P->HeavyAttack(); At=W->GetTimeSeconds(); Phase=7; return false;
  }
  if(Phase==7) { if(W->GetTimeSeconds()-At<.4f) return false; FCultMobaMechanicsTestAccess::Aim(P,P->FeetLocation()+FVector(7000,0,6500)); P->ReleaseHeavy(); At=W->GetTimeSeconds(); Phase=8; return false; }
  if(Phase==8) {
   if(W->GetTimeSeconds()-At<2.f) return false;
   T->TestTrue(TEXT("Missed thrown minion survives and leaves transit"),Minion.IsValid()&&Minion->Health>0&&!Minion->IsInTransit());
   T->TestEqual(TEXT("A miss halves remaining health without healing a wounded minion"),Minion->Health,40.f);
   T->TestTrue(TEXT("Missed minion lands within 45m of pickup"),FVector::Dist2D(Minion->GetActorLocation(),Pickup)<=4501);
   T->TestTrue(TEXT("Missed minion lands grounded"),FMath::Abs(Minion->GetActorLocation().Z-153)<10);
   T->TestTrue(TEXT("Survivor is ordered back onto its original lane"),Minion->RejoiningLane&&Minion->Lane==2);
   Land=Minion->GetActorLocation(); Minion->Speed=600; At=W->GetTimeSeconds(); Phase=9; return false;
  }
  if(Phase==9) {
   if(W->GetTimeSeconds()-At<.8f) return false;
   T->TestTrue(TEXT("Surviving minion really resumes lane movement"),FVector::Dist2D(Land,Minion->GetActorLocation())>150);
   Minion->SetActorLocation(FVector(-30000,-17000,153)); FCultMobaMechanicsTestAccess::Ready(P);
   Minion=FCultMobaMechanicsTestAccess::Spawn(M,1,FVector(2500,18000,0)); Minion->Speed=0;
   FCultMobaMechanicsTestAccess::Aim(P,Minion->GetActorLocation()); P->HeavyAttack(); T->TestTrue(TEXT("Interruption fixture begins held"),Minion->IsHeldBy(P));
   FCultMobaMechanicsTestAccess::Cancel(P); T->TestFalse(TEXT("Stunning the caster releases its minion"),Minion->IsInTransit()); T->TestEqual(TEXT("Interrupted minion survives wounded"),Minion->Health,60.f);
   P->ReleaseHeavy(); Minion->SetActorLocation(FVector(-30000,-17000,153)); FCultMobaMechanicsTestAccess::Ready(P);
   auto* Friendly=FCultMobaMechanicsTestAccess::Spawn(M,0,FVector(2500,18000,0)); Friendly->Speed=0; FCultMobaMechanicsTestAccess::Aim(P,Friendly->GetActorLocation()); P->HeavyAttack();
   T->TestFalse(TEXT("RMB cannot steal a friendly wave"),Friendly->IsInTransit()); T->TestEqual(TEXT("Refused pickup spends no cooldown"),FCultMobaMechanicsTestAccess::Cooldown(P),0.f); P->ReleaseHeavy();
   Friendly->Destroy();
   FCultMobaMechanicsTestAccess::Aim(P,P->FeetLocation()+FVector(4700,0,0));
   P->HeavyAttack();
   T->TestTrue(TEXT("RMB lifts a rock when aiming off living targets"),P->HoldingRock());
   T->TestTrue(TEXT("RMB starts the actual two-arm lift pose"),P->AttackPlayback().Move==CultAttackMotion::EMove::RockLift);
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==P&&It->IsGuarding()) TestRock=*It;
   if(!TestRock.IsValid()) return true;
   At=W->GetTimeSeconds(); Phase=10; return false;
  }
  if(Phase==10&&W->GetTimeSeconds()-At>3.55f) {
   if(!TestRock.IsValid()) { T->AddError(TEXT("The lifted rock disappeared before its guard window")); return true; }
   const FVector Chest=TestRock->GetActorLocation();
   T->TestTrue(TEXT("The live upper body stays braced with the collider raised"),P->AttackFrame().Limbs.X<-35);
   FHitResult Cover;
   FCollisionQueryParams CoverQuery(SCENE_QUERY_STAT(RockGuardFixture),false,P);
   W->LineTraceSingleByChannel(Cover,Chest+FVector(3600,0,0),Chest-FVector(2500,0,0),ECC_Visibility,CoverQuery);
   T->TestTrue(TEXT("The held boulder is the actual collider at chest height"),Cover.GetActor()==TestRock.Get());
   const FVector Old=TestRock->GetActorLocation();
   P->ReleaseHeavy();
   T->TestFalse(TEXT("Releasing RMB removes the guard"),P->HoldingRock());
   T->TestTrue(TEXT("RMB release plays the lowering motion"),P->AttackPlayback().Move==CultAttackMotion::EMove::Lower);
   T->TestTrue(TEXT("RMB release disarms the falling rock"),TestRock.IsValid()&&!TestRock->Hit);
   T->TestEqual(TEXT("Dropping the boulder leaves the rock ready"),FCultMobaMechanicsTestAccess::Cooldown(P),0.f);
   Pickup=Old; At=W->GetTimeSeconds(); Phase=11; return false;
  }
  if(Phase==11&&W->GetTimeSeconds()-At>.22f) {
   T->TestTrue(TEXT("Dropped rock moves downward without throwing forward"),
    TestRock.IsValid()&&TestRock->GetActorLocation().Z<Pickup.Z-40.f&&FVector::Dist2D(TestRock->GetActorLocation(),Pickup)<10);
   FCultMobaMechanicsTestAccess::Aim(P,P->FeetLocation()+FVector(4700,0,0));
   P->HeavyAttack();
   T->TestTrue(TEXT("Cthulhu can immediately raise another rock after a drop"),P->HoldingRock());
   At=W->GetTimeSeconds(); Phase=12; return false;
  }
  if(Phase==12&&W->GetTimeSeconds()-At>.4f) {
   Victim->RelocateMatch(FVector(4800,18000,0));
   FCultMobaMechanicsTestAccess::Aim(P,Victim->GetActorLocation());
   Before=Victim->GetHealth();
   P->Smash();
   T->TestFalse(TEXT("LMB releases the live rock projectile"),P->HoldingRock());
   T->TestTrue(TEXT("Throwing the boulder starts its cooldown"),FCultMobaMechanicsTestAccess::Cooldown(P)>0);
   T->TestTrue(TEXT("The throw uses its own release pose"),P->AttackPlayback().Move==CultAttackMotion::EMove::RockThrow);
   T->TestEqual(TEXT("The throw cue matches the rock windup"),P->AttackPlayback().Contact,.10f);
   T->TestEqual(TEXT("A thrown rock takes time to hit"),Victim->GetHealth(),Before);
   P->ReleaseHeavy();
   At=W->GetTimeSeconds(); Phase=13; return false;
  }
  if(Phase==13&&W->GetTimeSeconds()-At>1.f) {
   T->TestTrue(TEXT("The aimed boulder lands a balanced hit"),Victim->GetHealth()<Before&&Victim->GetHealth()>Before-23);
   FCultMobaMechanicsTestAccess::Ready(P);
   FCultMobaMechanicsTestAccess::Aim(P,P->FeetLocation()+FVector(4700,0,0));
   P->HeavyAttack();
   ACultSkillAttack* Guard=nullptr;
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==P&&It->IsGuarding()) Guard=*It;
   if(T->TestNotNull(TEXT("A third rock is held for durability checks"),Guard)) {
    Guard->DamageGuard(20.f);
    T->TestTrue(TEXT("The boulder withstands a single hit"),P->HoldingRock());
    Guard->DamageGuard(40.f);
    T->TestTrue(TEXT("Enough damage shatters the boulder"),Guard->IsActorBeingDestroyed());
   }
   P->ReleaseHeavy();
   Victim->RelocateMatch(FVector(-20000,-18000,0));
   const FTransform Spawn(P->FeetLocation()+FVector(4400,0,0));
   AcidCaster=W->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   if(!AcidCaster.IsValid()) { T->AddError(TEXT("Could not spawn ranged fixture")); return true; }
   AcidCaster->ConfigureBot(TEXT("mother_hydra"),1);
   AcidCaster->FinishSpawning(Spawn);
   AcidCaster->GetCharacterMovement()->DisableMovement();
   AcidCaster->SetBotAim(P);
   FCultMobaMechanicsTestAccess::SubstituteOpponent(M,Victim.Get(),AcidCaster.Get());
   Before=P->GetHealth();
   AcidCaster->Smash();
   AcidCaster->Smash();
   T->TestTrue(TEXT("Acid spit triggers its ranged pose"),AcidCaster->AttackPlayback().Move==CultAttackMotion::EMove::Primary);
   T->TestEqual(TEXT("Acid spit pose releases on the actual windup"),AcidCaster->AttackPlayback().Contact,.25f);
   int32 Shots=0;
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==AcidCaster.Get()&&It->Gravity>0) ++Shots;
   T->TestEqual(TEXT("The ranged LMB launches one gravity-driven spit through its cooldown"),Shots,1);
   T->TestFalse(TEXT("Ranged LMB never starts a melee sweep"),AcidCaster->IsSwinging());
   At=W->GetTimeSeconds(); Phase=14; return false;
  }
  if(Phase==14&&W->GetTimeSeconds()-At>1.32f) {
   DirectHealth=P->GetHealth();
   T->TestTrue(TEXT("The ballistic spit reaches the aimed hero for limited direct damage"),DirectHealth<Before&&DirectHealth>Before-9);
   Phase=15; return false;
  }
  if(Phase==15&&W->GetTimeSeconds()-At>3.7f) {
   T->TestTrue(TEXT("Corrosion adds limited delayed health damage"),P->GetHealth()<DirectHealth&&P->GetHealth()>Before-13);
   DirectHealth=P->GetHealth(); Phase=16; return false;
  }
  if(Phase==16&&W->GetTimeSeconds()-At>4.55f) {
   T->TestEqual(TEXT("The corrosion expires"),P->GetHealth(),DirectHealth);
   FCultMobaMechanicsTestAccess::SubstituteOpponent(M,AcidCaster.Get(),Victim.Get());
   AcidCaster->Destroy();
   return true;
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaMechanicsTest,"CultUlhu.Menu.MobaMechanics",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaMechanicsTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultMobaMechanicsObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
