#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CultUlhuPlayerPawn.h"
#include "CultKitRuntime.h"
#include "GameFramework/RootMotionSource.h"
#include "CultUlhuGameInstance.h"
#include "CultPopulation.h"
#include "CultFeralBeast.h"
#include "Components/CapsuleComponent.h"
#include "CultSkillAttack.h"
#include "CultCorpse.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "CultLivingLandscape.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#endif

struct FCultCombatFeelTestAccess {
 static void VerifyHeroCorpse(FAutomationTestBase* T,ACultUlhuPlayerPawn* P) {
  const FVector Before=P->GetActorLocation(),Scale=P->GetActorScale3D();
  auto* Corpse=ACultCorpse::Leave(P); T->TestNotNull(TEXT("Hero death creates a persistent body"),Corpse); if(!Corpse) return;
  FHitResult Ground; FCollisionQueryParams Q(SCENE_QUERY_STAT(HeroCorpseGround),false,P); Q.AddIgnoredActor(Corpse);
  const bool Found=P->GetWorld()->LineTraceSingleByObjectType(Ground,P->FeetLocation()+FVector(0,0,80),P->FeetLocation()-FVector(0,0,20000),FCollisionObjectQueryParams(ECC_WorldStatic),Q);
  T->TestTrue(TEXT("Corpse fixture has actual supporting terrain"),Found);
  TArray<UStaticMeshComponent*> Meshes; Corpse->GetComponents(Meshes); float SurfaceMin=MAX_flt; int Spheres=0;
  for(auto* Part:Meshes) if(Part->GetStaticMesh()&&Part->GetStaticMesh()->GetPathName()==TEXT("/Engine/BasicShapes/Sphere.Sphere")) {
   const auto Transform=Part->GetComponentTransform(); const float R=Part->GetStaticMesh()->GetBounds().BoxExtent.GetMax();
   const float X=Transform.TransformVector(FVector(R,0,0)).Z,Y=Transform.TransformVector(FVector(0,R,0)).Z,Z=Transform.TransformVector(FVector(0,0,R)).Z;
   const float Center=Transform.TransformPosition(Part->GetStaticMesh()->GetBounds().Origin).Z;
   const float Support=FMath::Sqrt(X*X+Y*Y+Z*Z);
   SurfaceMin=FMath::Min(SurfaceMin,Center-Support); ++Spheres;
   T->TestEqual(TEXT("Resting corpse meshes never block movement or attacks"),Part->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
  }
  T->AddInfo(FString::Printf(TEXT("Corpse source %s feet %s final actor %s floorZ %.2f"),*Before.ToCompactString(),*P->FeetLocation().ToCompactString(),*Corpse->GetActorLocation().ToCompactString(),Ground.ImpactPoint.Z));
  T->AddInfo(FString::Printf(TEXT("Hero corpse true sphere-surface clearance %.2fcm, copied sphere parts %d"),SurfaceMin-Ground.ImpactPoint.Z,Spheres));
  T->TestTrue(TEXT("The actual stretched sphere rests on the floor, not its conservative rotated box"),Found&&FMath::IsNearlyEqual(SurfaceMin,float(Ground.ImpactPoint.Z)+3.f,2.f));
  T->TestEqual(TEXT("Dead vessel retains its body without the two living eye lights"),Spheres,1);
  T->TestEqual(TEXT("Editor camera helpers are excluded from the dead body"),Meshes.Num(),1);
  T->TestTrue(TEXT("Creating a corpse preserves the source's location and canonical scale"),P->GetActorLocation().Equals(Before)&&P->GetActorScale3D().Equals(Scale));
  Corpse->Destroy(); P->Tags.Remove(TEXT("CorpseRecorded"));
 }
 static void MeasureProjectileDelivery(FAutomationTestBase* T,ACultUlhuPlayerPawn* P) {
  FString CSV=TEXT("character,ability,shape,spawned_count,speed_cm_s,radius_cm,reach_cm,windup_s,full_flight_s,canonical_walk_cm_s,canonical_sprint_cm_s\n");
  int Spells=0;
  for(const auto& Entry:UCultUlhuGameInstance::Roster()) {
   cultulhu::CharacterDef D; if(!CultCanonicalKit::Load(Entry.Id,D)) { T->AddError(TEXT("Could not read released source kit")); continue; }
   FCultKitRuntime Delivery(P); Delivery.Def=D;
   for(int Slot=0;Slot<3;++Slot) {
    const auto& Spell=Slot==0?D.qAbility:Slot==1?D.fAbility:D.rAbility; if(Spell.effectKind!="projectile") continue;
    TSet<ACultSkillAttack*> Existing; for(TActorIterator<ACultSkillAttack> It(P->GetWorld());It;++It) Existing.Add(*It);
    T->TestTrue(TEXT("Released source projectile creates native delivery"),Delivery.LaunchSpatial(Spell,Slot,P->GetActorLocation()+FVector(Spell.range*100,0,0)));
    TArray<ACultSkillAttack*> Spawned; for(TActorIterator<ACultSkillAttack> It(P->GetWorld());It;++It) if(It->GetOwner()==P&&!Existing.Contains(*It)) Spawned.Add(*It);
    T->TestTrue(TEXT("A sourced delivery actually exists before any tick"),Spawned.Num()>0);
    if(Spawned.Num()) {
     const auto* A=Spawned[0]; const bool Beam=A->Shape==ACultSkillAttack::EShape::Beam;
     const float Walk=D.moveSpeed*100*FMath::Sqrt(Entry.HeightM/1.8f)*1.25f;
     T->TestTrue(TEXT("Every measured projectile has a positive reaction delay and travel extent"),A->Windup>0&&A->Reach>0&&(Beam||A->Speed>0));
     CSV+=FString::Printf(TEXT("%s,%s,%s,%d,%.1f,%.1f,%.1f,%.3f,%.3f,%.1f,%.1f\n"),*Entry.Id,UTF8_TO_TCHAR(Spell.id.c_str()),Beam?TEXT("beam"):TEXT("bolt"),Spawned.Num(),Beam?0.f:A->Speed,A->Radius,A->Reach,A->Windup,Beam?0.f:A->Reach/A->Speed,Walk,Walk*1.85f); ++Spells;
    }
    for(auto* A:Spawned) A->Destroy();
   }
  }
  const auto Directory=FPaths::ProjectSavedDir()/TEXT("Reports"); IFileManager::Get().MakeDirectory(*Directory,true);
  T->TestTrue(TEXT("Runtime delivery measurements are saved"),FFileHelper::SaveStringToFile(CSV,*(Directory/TEXT("ReleasedProjectileDelivery.csv"))));
  T->AddInfo(FString::Printf(TEXT("Measured %d released projectile spells from live native attack actors; source kit statistics were unchanged"),Spells));
 }
 static float Stamina(ACultUlhuPlayerPawn* P) { return P->Kit_->Stamina.value; }
 static void Restore(ACultUlhuPlayerPawn* P,float Hp,float Stamina) {
  const float Difference=Hp-P->GetHealth(); if(Difference>0) P->Kit_->Heal(Difference); else if(Difference<0) P->Kit_->Damage(-Difference);
  P->Kit_->Stamina.value=Stamina; P->Kit_->Combo.reset(); P->Health_=P->Kit_->Hp();
  P->SmashCooldown_=P->SwingTime_=P->MeleeBuffer_=P->DashTime_=P->DashCooldown_=P->EvadeTime_=0;
  P->HurtFeedbackTime_=P->EvadeConfirmTime_=P->KillConfirmTime_=P->CombatFocusTime_=P->HitConfirmTime_=0;
  P->ImpactTexts_.Reset(); P->GetCharacterMovement()->RemoveRootMotionSource(TEXT("CultDash")); P->GetCharacterMovement()->StopMovementImmediately();
 }
 static FString Describe(ACultUlhuPlayerPawn* P) { return FString::Printf(TEXT("CD %.3f swing %.3f buffer %.3f stop %d stun %d fear %d"),P->SmashCooldown_,P->SwingTime_,P->MeleeBuffer_,P->Control_.Stop,P->Control_.Stun,P->Control_.Fear); }
 static void Ready(ACultUlhuPlayerPawn* P,float Cooldown=0) { P->SmashCooldown_=Cooldown; P->SwingTime_=0; P->MeleeBuffer_=0; }
 static void ResetDash(ACultUlhuPlayerPawn* P) { P->DashCooldown_=0; }
 static void Chase(ACultFeralBeast* B) { B->State_=ACultFeralBeast::EState::Chase; B->AttackCooldown_=B->AttackAnimTime_=B->RecoveryTime_=0; }
 static void Swipe(ACultFeralBeast* B,FVector P) { B->BeginSwipe(P); }
 static void LongWindup(ACultFeralBeast* B) { B->AttackAnimTime_=3.5f; }
};
class FCultCombatFeelObservation:public IAutomationLatentCommand {
public:
 FCultCombatFeelObservation(FAutomationTestBase* T,bool Capture):Test(T),Visual(Capture),Started(FPlatformTime::Seconds()) {}
 bool Update() override {
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World();
  if(!W) { if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("No game world for combat feel")); return true; } return false; }
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Entered) { auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->OpenCharacterSelect(ECultPlayMode::FreeRoam); GI->SetInitialBeliefs({11,5,6}); GI->StartPlay(); Entered=true; }
   if(FPlatformTime::Seconds()-Started>40) { Test->AddError(TEXT("Combat feel test did not enter the world")); return true; } return false;
  }
  auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0));
  if(!P||W->GetTimeSeconds()<2) return false;
  auto* PC=W->GetFirstPlayerController();
  const float Now=W->GetTimeSeconds();
  if(Phase==0) {
   SavedHp=P->GetHealth(); SavedStamina=FCultCombatFeelTestAccess::Stamina(P);
   PC->bShowMouseCursor=false; FCultCombatFeelTestAccess::Ready(P);
   P->Smash(); Test->TestTrue(TEXT("A click starts a committed physical swing"),P->IsSwinging());
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==P&&It->Shape==ACultSkillAttack::EShape::Sweep) FirstAngle=It->HalfAngle;
   Test->AddInfo(FString::Printf(TEXT("First physical melee angle %.1f at %.3f frame dt %.3f"),FirstAngle,Now,W->GetDeltaSeconds()));
   At=Now; Phase=1; return false;
  }
  if(Phase==1&&Now-At>.46f) {
   P->Smash(); P->Smash(); Test->TestTrue(TEXT("An early click queues one follow-up near recovery's end"),P->HasBufferedMelee());
   At=Now; Phase=2; return false;
  }
  if(Phase==2&&Now-At>.20f) {
   Test->TestTrue(TEXT("The queued click fires when recovery ends"),P->MeleeRecovery()>.35f);
   Test->TestFalse(TEXT("The follow-up consumes its buffer"),P->HasBufferedMelee());
   int Count=0; bool Opposite=false; for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==P&&It->Shape==ACultSkillAttack::EShape::Sweep) {
    ++Count; Opposite|=It->HalfAngle*FirstAngle<0;
    Test->AddInfo(FString::Printf(TEXT("Follow-up physical actor angle %.1f windup left %.3f"),It->HalfAngle,It->TimeUntilActive()));
   }
   Test->AddInfo(FString::Printf(TEXT("Follow-up observation %.3fs after buffering, frame dt %.3f, %d physical actors: %s"),Now-At,W->GetDeltaSeconds(),Count,*FCultCombatFeelTestAccess::Describe(P)));
   Test->TestEqual(TEXT("Exactly one follow-up physical swing exists after buffering"),Count,1);
   Test->TestTrue(TEXT("Consecutive slashes alternate their physical sweep direction"),Opposite);
   At=Now; Phase=3; return false;
  }
  if(Phase==3&&Now-At>.8f) {
   FCultCombatFeelTestAccess::VerifyHeroCorpse(Test,P);
   const FVector OriginalScale=P->GetActorScale3D(); const FRotator OriginalRotation=P->GetActorRotation();
   for(int I=0;I<3;++I) { const float Height=I==0?20.f:I==1?28.f:30.f; P->SetActorScale3D(FVector(Height/1.8f)); P->SetActorRotation(FRotator(0,45+I*75,0)); FCultCombatFeelTestAccess::VerifyHeroCorpse(Test,P); }
   P->SetActorScale3D(OriginalScale); P->SetActorRotation(OriginalRotation);
   P->ToggleCamera(); FCultCombatFeelTestAccess::VerifyHeroCorpse(Test,P); P->ToggleCamera();
   FCultCombatFeelTestAccess::MeasureProjectileDelivery(Test,P);
   Test->TestEqual(TEXT("Repeated clicks do not create an infinite auto-attack queue"),P->MeleeRecovery(),0.f);
   const FVector Arena=P->FeetLocation()+FVector(15000,-15000,50000);
   FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   auto* Target=W->SpawnActor<ACultPopulation>(Arena,FRotator::ZeroRotator,Params); Target->ConfigureBred(-1,100); Target->GetCharacterMovement()->DisableMovement();
   ACultSkillAttack::Hurt(Target,30,P,Arena);
   Test->TestEqual(TEXT("Feedback follows actual health loss"),Target->GetHealth(),70.f);
   Test->TestTrue(TEXT("A real contact generates a damage receipt"),P->ImpactTexts().Num()>0&&P->ImpactTexts().Last().Amount==30);
   ACultSkillAttack::Hurt(Target,1000,P,Arena);
   Test->TestTrue(TEXT("Overkill reports remaining health, not fictitious damage"),P->ImpactTexts().Last().Amount==100&&P->ImpactTexts().Last().Killed);
   Test->TestTrue(TEXT("An actual elimination has distinct feedback"),P->KillConfirmTime()>0);
   auto* Civilian=W->SpawnActor<ACultPopulation>(Arena+FVector(800,0,0),FRotator::ZeroRotator,Params);
   ACultSkillAttack::Hurt(Civilian,135,P,Civilian->GetActorLocation());
   Test->TestTrue(TEXT("One-hit civilians report their real one HP"),P->ImpactTexts().Last().Amount==1&&P->ImpactTexts().Last().Killed);
   FCultCombatFeelTestAccess::Ready(P,.1f); P->Smash(); P->SetControlState(true,false,0,P->GetActorLocation(),true,519);
   P->Tick(.01f); Test->TestFalse(TEXT("Stun clears queued melee instead of firing it later"),P->HasBufferedMelee());
   P->SetControlState(false,false,1,P->GetActorLocation(),false,519); P->Tick(.01f);
   FCultCombatFeelTestAccess::Ready(P); P->Smash(); FCultCombatFeelTestAccess::ResetDash(P);
   Test->TestTrue(TEXT("A dash can cancel a committed melee attack"),P->TryDash(FVector::RightVector));
   int Active=0; for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==P&&It->Shape==ACultSkillAttack::EShape::Sweep) ++Active;
   Test->TestEqual(TEXT("Dash cancellation cannot drag a damaging arm across the map"),Active,0);
   Before=P->GetHealth(); P->ReceiveDamage(12);
   Test->TestEqual(TEXT("A confirmed dodge prevents damage"),P->GetHealth(),Before);
   Test->TestTrue(TEXT("A prevented hit displays dodge feedback"),P->EvadeConfirmTime()>0);

   FTransform Transform(Arena+FVector(2500,0,0));
   auto* B=W->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   B->SetSpecies(TEXT("ossified_brute")); B->FinishSpawning(Transform);
   FCultCombatFeelTestAccess::Swipe(B,B->GetActorLocation()+FVector(200,0,0));
   TWeakObjectPtr<ACultSkillAttack> Windup;
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==B) Windup=*It;
   Test->TestTrue(TEXT("Enemy attacks expose an actual readable windup"),Windup.IsValid()&&Windup->IsWindingUp()&&Windup->TimeUntilActive()>.5f);
   B->ApplyDamage(135); Test->TestTrue(TEXT("A brute resists one ordinary hit"),Windup.IsValid()&&B->StaggerRemaining()==0);
   B->ApplyDamage(270); Test->TestTrue(TEXT("Enough real damage breaks poise during windup"),!Windup.IsValid()&&B->StaggerRemaining()>0);
   B->Tick(.5f); B->ApplyDamage(135); Test->TestEqual(TEXT("Poise cooldown prevents permanent stagger locking"),B->StaggerRemaining(),0.f);
   FCultCombatFeelTestAccess::Swipe(B,B->GetActorLocation()+FVector(200,0,0));
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==B) It->Tick(1.f);
   B->Tick(1.f); const FVector Position=B->GetActorLocation();
   Test->TestTrue(TEXT("A heavy swipe leaves a punishable recovery window"),B->RecoveryRemaining()>.8f);
   B->Tick(.5f); Test->TestTrue(TEXT("A recovering enemy stays committed rather than tracking the player"),B->GetActorLocation().Equals(Position,1.f));
   B->Destroy(); At=Now; Phase=4; return false;
  }
  if(Phase==4&&Now-At>.28f) {
   const float HP=P->GetHealth(); P->ReceiveDamage(5);
   Test->TestTrue(TEXT("Dodge immunity expires and real hits have incoming feedback"),P->GetHealth()<HP&&P->HurtFeedbackTime()>0);
   // Trace the actual hero capsule at creature arm height, then exercise native approach and collision.
   FHitResult Surface; FCollisionQueryParams Query(SCENE_QUERY_STAT(CultLegReach),false);
   const FVector Feet=P->FeetLocation();
   const bool HasSurface=P->GetCapsuleComponent()->LineTraceComponent(Surface,Feet+FVector(5000,0,110),Feet+FVector(0,0,110),Query);
   Test->TestTrue(TEXT("The hero has a real collidable lower-body surface"),HasSurface);
   if(HasSurface) {
    FTransform T(Surface.ImpactPoint+FVector(400,0,0)); auto* Ghoul=W->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Ghoul->SetSpecies(TEXT("skittering_ghoul")); Ghoul->FinishSpawning(T); FCultCombatFeelTestAccess::Chase(Ghoul);
    Ghoul->Tick(.016f);
    int Early=0; for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==Ghoul) ++Early;
    Test->TestEqual(TEXT("Small enemies approach the legs before committing an out-of-range swipe"),Early,0);
    const float HH=Ghoul->GetSimpleCollisionHalfHeight(); Ghoul->SetActorLocation(FVector(Surface.ImpactPoint.X+140,Feet.Y,Feet.Z+HH));
    Ghoul->Tick(.016f); TArray<ACultSkillAttack*> Swipes;
    for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==Ghoul) Swipes.Add(*It);
    Test->TestTrue(TEXT("A small enemy commits after reaching the lower body"),Swipes.Num()>0);
    Before=P->GetHealth(); LegEnemy=Ghoul; LegContact=MakeShared<bool>(false);
    for(auto* Swipe:Swipes) {
     auto Previous=MoveTemp(Swipe->Hit); TWeakObjectPtr<ACultUlhuPlayerPawn> Player(P); auto Contact=LegContact;
     Swipe->Hit=[Previous=MoveTemp(Previous),Player,Contact](AActor* Other,FVector Point) {
      const float Hp=Player.IsValid()?Player->GetHealth():0;
      if(Previous) Previous(Other,Point);
      if(Player.IsValid()&&Other==Player.Get()&&Player->GetHealth()<Hp) *Contact=true;
     };
    }
    At=Now; Phase=7; return false;
   }
   At=Now; Phase=8; return false;
  }
  if(Phase==7&&Now-At>.75f) {
   Test->TestTrue(TEXT("A committed creature swipe physically reaches a tall standing player"),LegContact.IsValid()&&*LegContact&&P->GetHealth()<Before);
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==LegEnemy.Get()) It->Destroy();
   if(LegEnemy.IsValid()) LegEnemy->Destroy(); Phase=8; At=Now; return false;
  }
  if(Phase==8) {
   if(!Visual) { FCultCombatFeelTestAccess::Restore(P,SavedHp,SavedStamina); return true; }
   // A nearby authored creature and a deliberately extended test-only windup for visual inspection.
   FVector Ground; bool Found=false;
   for(int I=0;I<24&&!Found;++I) {
    const float Angle=I*PI/12; FVector Candidate=P->FeetLocation()+FVector(FMath::Cos(Angle)*2400,FMath::Sin(Angle)*2400,0);
    Found=ACultLivingLandscape::Surface(W,Candidate,Ground,.9f);
   }
   if(!Found) { Test->AddError(TEXT("No visible terrain for combat presentation screenshot")); return true; }
   FTransform T(Ground+FVector(0,0,200)); auto* B=W->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   B->SetSpecies(TEXT("ossified_brute")); B->FinishSpawning(T); VisualBeast=B;
   FCultCombatFeelTestAccess::Swipe(B,P->FeetLocation()+FVector(0,0,100)); FCultCombatFeelTestAccess::LongWindup(B);
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==B) It->Windup=3.3f;
   Camera=W->SpawnActor<ACameraActor>(Ground+FVector(-950,-950,650),FRotator::ZeroRotator);
   Camera->SetActorRotation((B->GetActorLocation()-Camera->GetActorLocation()).Rotation()); PC->SetViewTarget(Camera);
   ACultSkillAttack::Hurt(B,27,P,B->GetActorLocation()+FVector(0,0,80)); At=Now; Phase=5; return false;
  }
  if(Phase==5&&Now-At>.16f) {
   FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/CombatFeel.png")),true,false);
   At=Now; Phase=6; return false;
  }
  if(Phase==6&&Now-At>1.f) {
   Test->TestTrue(TEXT("Combat presentation screenshot saved"),FPaths::FileExists(FPaths::ProjectSavedDir()/TEXT("Screenshots/CombatFeel.png")));
   PC->SetViewTarget(P); if(Camera) Camera->Destroy(); if(VisualBeast.IsValid()) { for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==VisualBeast.Get()) It->Destroy(); VisualBeast->Destroy(); }
   FCultCombatFeelTestAccess::Restore(P,SavedHp,SavedStamina); return true;
  }
  if(Now-At>10) { Test->AddError(TEXT("Combat feel phase timed out")); return true; } return false;
 }
private:
 FAutomationTestBase* Test; bool Visual,Entered=false; double Started; float At=0,FirstAngle=0,Before=0,SavedHp=0,SavedStamina=0; int Phase=0;
 ACameraActor* Camera=nullptr; TWeakObjectPtr<ACultFeralBeast> VisualBeast,LegEnemy; TSharedPtr<bool> LegContact;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultCombatFeelTest,"CultUlhu.CombatFeel",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCultCombatFeelTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultCombatFeelObservation(this,false)); return true; }
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultCombatFeelEditorTest,"CultUlhu.Menu.CombatFeel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultCombatFeelEditorTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); ADD_LATENT_AUTOMATION_COMMAND(FCultCombatFeelObservation(this,true)); return true; }
#endif
#endif
