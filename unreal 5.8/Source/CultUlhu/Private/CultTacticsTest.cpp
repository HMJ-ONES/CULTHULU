#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "CultAutomationPIE.h"
#include "CultMatchState.h"
#include "CultKitRuntime.h"
#include "CultSkillAttack.h"
#include "CultUlhuGameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "CultCharacterModel.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"

struct FCultTacticsTestAccess {
 static void VerifyAim(FAutomationTestBase* T,ACultMatchDirector* M) {
  auto* W=M->GetWorld(); M->SetActorTickEnabled(false); M->State_->Warmup=0;
  auto* P=M->State_->Fighters[0].Pawn.Get(); auto* PC=W->GetFirstPlayerController();
  for(auto& F:M->State_->Fighters) { F.Shield=0; if(auto* Other=F.Pawn.Get()) { Other->bBotControlled_=false; Other->SetActorTickEnabled(false); Other->RelocateMatch(FVector(-30000,-25000,0)); } }
  P->RelocateMatch(FVector(0,0,60000)); PC->SetControlRotation(FRotator(0,0,0));
  T->TestFalse(TEXT("Human movement no longer turns the body away from the aiming direction"),P->GetCharacterMovement()->bOrientRotationToMovement);
  for(int View=0;View<2;++View) {
   if((View==0)!=P->IsThirdPerson()) P->ToggleCamera();
   P->SpringArm_->TickComponent(.016f,LEVELTICK_All,nullptr);
   for(int Moving=0;Moving<2;++Moving) {
    P->GetCharacterMovement()->Velocity=Moving?FVector(-900,0,0):FVector::ZeroVector;
    auto Pick=P->CameraAim(); const FVector Ray=(Pick.TraceEnd-Pick.TraceStart).GetSafeNormal();
    const float Plane=FVector::DotProduct(P->GetActorLocation()-Pick.TraceStart,Ray)+1600;
    auto* Target=M->AddUnit(1,1,0,-1,Pick.TraceStart+Ray*Plane-FVector(0,0,153),1000,0,0,0); Target->SetActorTickEnabled(false);
    Pick=P->CameraAim(); T->TestEqual(TEXT("The screen-center ray acquires the intended physical target"),Pick.GetActor(),static_cast<AActor*>(Target));
    P->Kit_->SetCooldown(0,0); P->AbilityQ(); ACultSkillAttack* Bolt=nullptr;
    for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==P&&It->Shape==ACultSkillAttack::EShape::Bolt) Bolt=*It;
    T->TestNotNull(TEXT("Actual Q creates its authored projectile while stationary or backing up"),Bolt);
    if(Bolt) {
     const FVector Expected=(Pick.ImpactPoint-Bolt->GetActorLocation()).GetSafeNormal();
     T->TestTrue(TEXT("Projectile delivery aims at the crosshair in both camera views and movement directions"),FVector::DotProduct(Expected,Bolt->Direction)>.9999);
     Bolt->Tick(0); Bolt->Tick(1.2f); T->TestTrue(TEXT("The aimed projectile physically hits the crosshair target within its authored range"),Target->Health<1000);
    }
    P->SmashCooldown_=P->SwingTime_=0; P->Smash();
    for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==P&&It->Shape==ACultSkillAttack::EShape::Sweep) {
     const FVector Expected=(Pick.ImpactPoint-It->GetActorLocation()).GetSafeNormal();
     T->TestTrue(TEXT("Melee commits its arc toward the same crosshair target"),FVector::DotProduct(Expected,It->Direction)>.9999);
     const FVector Before=It->GetActorLocation(); P->SetActorRotation(FRotator(0,90,0)); It->Tick(0);
     T->TestTrue(TEXT("Body rotation cannot shift the committed arm origin sideways"),It->GetActorLocation().Equals(Before,.01));
    }
    for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==P) It->Destroy(); Target->Destroy(); P->SetActorRotation(FRotator::ZeroRotator);
   }
  }
  P->GetCharacterMovement()->Velocity=FVector::ZeroVector;
  for(const FString Id:{TEXT("mi_go"),TEXT("yig")}) {
   auto* Body=W->SpawnActor<ACultCharacterModel>(); T->TestTrue(TEXT("Approved animated body initializes: ")+Id,Body->Initialize(Id,true));
   for(const FString Role:{TEXT("idle"),TEXT("walk"),TEXT("run"),TEXT("jump"),TEXT("land"),TEXT("dash"),TEXT("attack"),TEXT("cast")}) {
    Body->SetOwner(P); Body->AttachToActor(P,FAttachmentTransformRules::KeepWorldTransform); ACultCharacterModel::Cue(P,Role,.5f);
    int Animated=0; for(const auto& Part:Body->Parts()) if(auto* Skeletal=Cast<USkeletalMeshComponent>(Part.Get())) { auto* Instance=Skeletal->GetSingleNodeInstance(); if(Instance&&Instance->GetCurrentAsset()) ++Animated; }
    T->TestEqual(TEXT("Every rig piece receives the requested gameplay animation: ")+Id+TEXT("/")+Role,Animated,Id==TEXT("yig")?3:1);
   }
   Body->Destroy();
  }
 }
 static void Verify(FAutomationTestBase* T,ACultMatchDirector* M) {
  M->SetActorTickEnabled(false); M->State_->Warmup=0; M->State_->Over=false;
  for(int I=0;I<M->State_->Fighters.Num();++I) {
   auto& F=M->State_->Fighters[I]; F.Shield=0;
   if(auto* P=F.Pawn.Get()) { P->bBotControlled_=false; P->RelocateMatch(FVector(-30000+I*2000,-24000,0)); P->GetCharacterMovement()->DisableMovement(); }
  }
  for(auto& U:M->State_->Units) if(U.IsValid()) { U->Range=0; U->Dps=0; U->Speed=0; }
  auto& F=M->State_->Fighters[1]; if(F.Pawn.IsValid()) F.Pawn->Destroy(); F.Pawn=nullptr; F.Kit=TEXT("mi_go"); M->SpawnFighter(1);
  auto* Bot=F.Pawn.Get(); auto* Enemy=M->State_->Fighters[5].Pawn.Get(); auto* Ally=M->State_->Fighters[0].Pawn.Get();
  F.Shield=0; F.Level=30; F.RouteStep=2; F.DomPath.Reset(); F.NextTacticalMove=999; F.Think=0;
  Bot->RelocateMatch(FVector(0,4800,0)); Enemy->RelocateMatch(FVector(0,3200,0)); Ally->RelocateMatch(FVector(1800,1000,0));
  F.Lane=1; T->TestTrue(TEXT("Mi-Go ranged spacing comes from the existing twenty-metre kit"),Bot->BotRangedReach()>=2000);
  M->BotThink(1,.016f);
  T->TestEqual(TEXT("Bot chooses a real opposing hero instead of fixating on the human"),Bot->GetBotAim(),static_cast<AActor*>(Enemy));
  if(M->Mode()==ECultPlayMode::Domination) {
   T->TestFalse(TEXT("Ranged support can fire from outside the seal while an ally captures"),ACultMatchDirector::InsideCapture(F.TacticalGoal));
   T->TestTrue(TEXT("Ranged support remains close enough to engage"),FVector::Dist2D(F.TacticalGoal,Enemy->FeetLocation())<3500);
   Ally->RelocateMatch(FVector(-22000,-22000,0)); Bot->RelocateMatch(FVector(0,4800,0)); F.Think=999; M->BotThink(1,.016f);
   T->TestTrue(TEXT("Without a teammate capturing the ranged bot returns to the objective"),ACultMatchDirector::InsideCapture(F.TacticalGoal));
  }
  // A threatened teammate attracts support even if another opponent is nearer.
  auto* Near=M->State_->Fighters[6].Pawn.Get(); Near->RelocateMatch(FVector(2500,4800,0));
  Enemy->RelocateMatch(FVector(0,1700,0)); Ally->RelocateMatch(FVector(0,1500,0)); F.Think=999;
  M->BotThink(1,.016f); T->TestEqual(TEXT("Bot prioritizes supporting the teammate's nearby opponent"),Bot->GetBotAim(),static_cast<AActor*>(Enemy));
  // Observe an actual tactical escape decision on supported ground.
  auto* Floor=M->GetWorld()->SpawnActor<AStaticMeshActor>(FVector(0,0,60000),FRotator::ZeroRotator);
  auto* Surface=Floor->GetStaticMeshComponent(); Surface->SetMobility(EComponentMobility::Movable); Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); Surface->SetCollisionProfileName(TEXT("BlockAll")); Floor->SetActorScale3D(FVector(400,400,1));
  Bot->RelocateMatch(FVector(0,0,60053)); Enemy->RelocateMatch(FVector(1000,0,60053)); Ally->RelocateMatch(FVector(-22000,-22000,0)); Near->RelocateMatch(FVector(20000,20000,0));
  Bot->DashCooldown_=Bot->DashTime_=0; Bot->Kit_->AdvanceCooldowns(100); F.Think=999; F.NextTacticalMove=0;
  M->BotThink(1,.016f);
  T->TestTrue(TEXT("A pressured ranged bot chooses a real tactical dash on safe ground"),Bot->IsDashing());
  T->TestTrue(TEXT("The escape dash moves away from the nearby opponent"),Bot->DashDirection_.X<0);
  T->TestEqual(TEXT("Dash cooldown follows this match mode"),Bot->CombatHUD().DashCooldown,M->Mode()==ECultPlayMode::Dotgoo?8.f:1.15f);
  Bot->GetCharacterMovement()->RemoveRootMotionSource(TEXT("CultDash")); Bot->DashTime_=0;
  T->AddInfo(FString::Printf(TEXT("Tactics mode %d: ranged reach %.0f, goal %s, dash cooldown %.2f"),int(M->Mode()),Bot->BotRangedReach(),*F.TacticalGoal.ToCompactString(),Bot->CombatHUD().DashCooldown));
 }
};
class FCultTacticsObservation:public IAutomationLatentCommand {
 FAutomationTestBase* T; ECultPlayMode Mode; bool Aim=false,Entered=false; double Start=FPlatformTime::Seconds();
public:
 FCultTacticsObservation(FAutomationTestBase* Test,ECultPlayMode M,bool CheckAim=false):T(Test),Mode(M),Aim(CheckAim) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>90) { T->AddError(TEXT("Tactics test timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Entered) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=Aim?TEXT("mi_go"):TEXT("cthulhu_avatar"); G->OpenCharacterSelect(Mode); G->OpenBeliefSelect(); Entered=true; } return false; }
  auto* M=ACultMatchDirector::Find(W); if(!M||M->Warmup()>0) return false;
  if(Aim) FCultTacticsTestAccess::VerifyAim(T,M); else FCultTacticsTestAccess::Verify(T,M); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDomTacticsTest,"CultUlhu.Menu.CombatTactics.Domination",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDomTacticsTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultTacticsObservation(this,ECultPlayMode::Domination)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaTacticsTest,"CultUlhu.Menu.CombatTactics.Moba",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaTacticsTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultTacticsObservation(this,ECultPlayMode::Dotgoo)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultAimPresentationTest,"CultUlhu.Menu.CrosshairAimAndMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultAimPresentationTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultTacticsObservation(this,ECultPlayMode::Domination,true)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
