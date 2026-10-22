#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "CultSkillAttack.h"
#include "CultKitRuntime.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace {
void RangedKey(APlayerController* PC,FKey Key,EInputEvent Event) {
 PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,Event,Event==IE_Released?0.f:1.f,false,FPlatformTime::Cycles64()));
}
}
class FCultRangedEncounterObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0,Case=0; double Began=FPlatformTime::Seconds(); float At=0,CastAt=0,Before=0;
 TWeakObjectPtr<ACultUlhuPlayerPawn> Target,Shooter; TWeakObjectPtr<ACultMatchDirector> Match; TWeakObjectPtr<ACultSkillAttack> Shot; TWeakObjectPtr<AStaticMeshActor> Wall;
 TUniquePtr<FCultKitRuntime> Delivery; TSharedPtr<int> Contacts; TSharedPtr<float> FirstContact; TSharedPtr<FVector> EndPoint; FVector Direction,Velocity,Start; float MaxTurn=0; bool Countered=false;
 FString CSV=TEXT("case,contact_count,damage,cast_vx,cast_vy,direction_x,direction_y,max_direction_change,displacement_cm,first_contact_s\n");
 const TCHAR* Name() const { return Case==0?TEXT("stationary"):Case==1?TEXT("steady_strafe"):Case==2?TEXT("dash_after_release"):Case==3?TEXT("cover_after_release"):TEXT("sprint_strafe"); }
 void ReleaseKeys(UWorld* W) { if(auto* PC=W->GetFirstPlayerController()) for(auto Key:{EKeys::D,EKeys::A,EKeys::LeftShift}) RangedKey(PC,Key,IE_Released); }
public:
 explicit FCultRangedEncounterObservation(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>100) { Test->AddError(TEXT("Moving-target encounter timed out")); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(Phase==0) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("mi_go"); G->OpenCharacterSelect(ECultPlayMode::Dotgoo); G->OpenBeliefSelect(); Phase=1; } return false;
  }
  auto* M=ACultMatchDirector::Find(W); if(!M||M->Fighters().Num()!=10||M->Warmup()>0||M->Elapsed()<4) return false;
  const float Now=W->GetTimeSeconds();
  if(Phase==1) {
   Match=M; Target=M->Fighters()[0].Pawn; Shooter=M->Fighters()[5].Pawn;
   Test->TestTrue(TEXT("Encounter uses the actual released Mi-Go kit on both sides"),Target.IsValid()&&Shooter.IsValid()&&Shooter->IsBot());
   if(!Target.IsValid()||!Shooter.IsValid()) return true;
   M->SetActorTickEnabled(false);
   for(const auto& F:M->Fighters()) if(F.Pawn.IsValid()) {
    F.Pawn->GetCharacterMovement()->StopMovementImmediately(); F.Pawn->GetCharacterMovement()->DisableMovement();
    if(F.Pawn!=Target&&F.Pawn!=Shooter) F.Pawn->SetActorEnableCollision(false);
   }
   M->AbilityAvailable(Shooter.Get(),0,true); M->OnAction(Target.Get());
   W->GetFirstPlayerController()->bShowMouseCursor=false;
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) It->Destroy(); Phase=2;
  }
  if(Phase==2) {
   ReleaseKeys(W); if(Wall.IsValid()) Wall->Destroy(); if(Shot.IsValid()) Shot->Destroy();
   Target->RespawnMatch(FVector(-900,Case? -900:0,0)); Target->MarkCombat();
   Shooter->RespawnMatch(FVector(-3000,0,0)); Shooter->SetBotAim(Target.Get());
   Shooter->GetCharacterMovement()->DisableMovement();
   Delivery=MakeUnique<FCultKitRuntime>(Shooter.Get()); Test->TestTrue(TEXT("Canonical delivery runtime initializes"),Delivery->Initialize(TEXT("mi_go")));
   Contacts=MakeShared<int>(0); FirstContact=MakeShared<float>(-1.f); EndPoint=MakeShared<FVector>(FVector(MAX_flt)); MaxTurn=0; Countered=false;
   if(Case) RangedKey(W->GetFirstPlayerController(),EKeys::D,IE_Pressed);
   if(Case==4) RangedKey(W->GetFirstPlayerController(),EKeys::LeftShift,IE_Pressed);
   At=Now; Phase=3; return false;
  }
  // Replay held input every observation frame: an offscreen viewport may
  // flush synthetic key state while focus changes. Movement still runs through
  // the real key polling, stamina, acceleration, capsule and character physics.
  if((Phase==3||Phase==4)&&Case>0) {
   auto* PC=W->GetFirstPlayerController();
   RangedKey(PC,Case==2&&Countered?EKeys::A:EKeys::D,IE_Pressed);
   if(Case==4) RangedKey(PC,EKeys::LeftShift,IE_Pressed);
  }
  if(Phase==3) {
   const float Desired=Case==4?4600.f:2300.f;
   if(Now-At<.4f||(Case&&Target->GetVelocity().Y<Desired)) { if(Now-At>3) { Test->AddError(FString::Printf(TEXT("Real strafe input never reached expected velocity: feet%s velocity%s maxSpeed%.1f mode%d keyD%d cursor%d canAct%d controller%d ignored%d"),*Target->FeetLocation().ToCompactString(),*Target->GetVelocity().ToCompactString(),Target->GetCharacterMovement()->MaxWalkSpeed,int(Target->GetCharacterMovement()->MovementMode),W->GetFirstPlayerController()->IsInputKeyDown(EKeys::D),W->GetFirstPlayerController()->bShowMouseCursor,M->CanAct(Target.Get()),Target->GetController()==W->GetFirstPlayerController(),W->GetFirstPlayerController()->IsMoveInputIgnored())); return true; } return false; }
   Before=Target->GetHealth(); Velocity=Target->GetVelocity(); Start=Target->FeetLocation();
   Test->TestTrue(TEXT("Actual source Q casts without bypassing its resource/cooldown path"),Delivery->Cast(0));
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==Shooter.Get()) Shot=*It;
   if(!Shot.IsValid()) { Test->AddError(TEXT("Canonical Q created no physical shot")); return true; }
   Test->TestEqual(TEXT("Needle retains the native fast, narrow profile"),Shot->Speed,4800.f); Test->TestEqual(TEXT("Needle radius is unchanged"),Shot->Radius,14.f);
   Direction=Shot->Direction; auto Original=Shot->Hit; auto Count=Contacts; auto ContactTime=FirstContact; const float ShotBegan=Now; auto Victim=Target;
   Shot->Hit=[Original,Count,ContactTime,ShotBegan,Victim](AActor* Hit,FVector Point) { if(Hit==Victim.Get()) { ++*Count; if(*ContactTime<0) *ContactTime=Hit->GetWorld()->GetTimeSeconds()-ShotBegan; } if(Original) Original(Hit,Point); };
   auto Finished=Shot->Finished; auto End=EndPoint; Shot->Finished=[Finished,End](FVector Point) { *End=Point; if(Finished) Finished(Point); };
   Test->TestEqual(TEXT("Casting has no instantaneous damage"),Target->GetHealth(),Before);
   CastAt=Now; Phase=4; return false;
  }
  if(Phase==4) {
   if(Shot.IsValid()) MaxTurn=FMath::Max(MaxTurn,float((Shot->Direction-Direction).Size()));
   if(!Countered&&Now-CastAt>(Case==2?.32f:.24f)&&(Case==2||Case==3)) {
    Test->TestTrue(TEXT("Counterplay begins after the actual windup has completed"),Shot.IsValid()&&!Shot->IsWindingUp());
    Countered=true;
    if(Case==2) {
     RangedKey(W->GetFirstPlayerController(),EKeys::D,IE_Released); RangedKey(W->GetFirstPlayerController(),EKeys::A,IE_Pressed);
     Test->TestTrue(TEXT("A real opposing dash can be started after projectile release"),Target->TryDash(FVector(0,-1,0)));
    } else {
     Wall=W->SpawnActor<AStaticMeshActor>(FVector(-1800,0,1500),FRotator::ZeroRotator); auto* Mesh=Wall->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); Mesh->SetWorldScale3D(FVector(1.2f,70,30)); Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    }
   }
   if(Now-CastAt<1.1f) return false;
   const float Damage=Before-Target->GetHealth(),Displacement=FVector::Dist2D(Start,Target->FeetLocation());
   Test->TestTrue(TEXT("The projectile never retargets after commitment"),MaxTurn<.00001f);
   if(Case==0||Case==1) {
    Test->TestEqual(FString::Printf(TEXT("%s receives one real canonical projectile contact"),Name()),*Contacts,1);
    Test->TestTrue(TEXT("A real contact produces native competitive damage"),Damage>0);
   } else {
    Test->TestEqual(FString::Printf(TEXT("%s avoids physical contact, not just health loss"),Name()),*Contacts,0);
    Test->TestEqual(TEXT("An avoided shot causes no damage"),Damage,0.f);
    if(Case==2) Test->TestTrue(TEXT("Dash evasion has expired by the observation"),!Target->IsEvading());
    if(Case==3) Test->TestTrue(TEXT("The shot actually finishes on the inserted wall"),EndPoint->X>-2000&&EndPoint->X<-1800);
   }
   CSV+=FString::Printf(TEXT("%s,%d,%.2f,%.1f,%.1f,%.5f,%.5f,%.8f,%.1f,%.3f\n"),Name(),*Contacts,Damage,Velocity.X,Velocity.Y,Direction.X,Direction.Y,MaxTurn,Displacement,*FirstContact);
   Test->AddInfo(FString::Printf(TEXT("%s contacts%d damage%.1f castVelocity%s direction%s turn%.8f movement%.1f firstContact%.3fs"),Name(),*Contacts,Damage,*Velocity.ToCompactString(),*Direction.ToCompactString(),MaxTurn,Displacement,*FirstContact));
   if(++Case<5) { Phase=2; return false; }
   ReleaseKeys(W); if(Wall.IsValid()) Wall->Destroy();
   Target->RespawnMatch(FVector(10000,0,0)); Shooter->RespawnMatch(FVector(-3000,0,0)); Shooter->SetBotAim(Target.Get());
   Delivery=MakeUnique<FCultKitRuntime>(Shooter.Get()); Delivery->Initialize(TEXT("mi_go")); const float StaminaBefore=Delivery->StaminaValue();
   Test->TestFalse(TEXT("A bot does not spend an aimed projectile on an unreachable body"),Delivery->Cast(0));
   Test->TestEqual(TEXT("An unreachable shot preserves its cooldown"),Delivery->Cooldown(0),0.f);
   Test->TestEqual(TEXT("An unreachable shot preserves its stamina"),Delivery->StaminaValue(),StaminaBefore);
   int Strays=0; for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==Shooter.Get()) ++Strays;
   Test->TestEqual(TEXT("Range rejection spawns no stray attack actor"),Strays,0);
   // Direction-only probe: even a supplied moving candidate must not provide
   // prediction or aim assistance to a human-controlled cast.
   const FVector OldVelocity=Shooter->GetCharacterMovement()->Velocity;
   Shooter->GetCharacterMovement()->Velocity=FVector(0,2500,0); Target->SetBotAim(Shooter.Get());
   FCultKitRuntime Manual(Target.Get()); Manual.Def=Delivery->Def; const FVector ManualPoint=Shooter->GetActorLocation();
   Manual.LaunchSpatial(Manual.Def.qAbility,0,ManualPoint);
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==Target.Get()) {
    Test->TestTrue(TEXT("Human casts keep the exact explicitly aimed direction"),It->Direction.Equals((ManualPoint-It->GetActorLocation()).GetSafeNormal(),.00001f)); It->Destroy();
   }
   Shooter->GetCharacterMovement()->Velocity=OldVelocity; Target->SetBotAim(nullptr);
   const auto Directory=FPaths::ProjectSavedDir()/TEXT("Reports"); IFileManager::Get().MakeDirectory(*Directory,true); FFileHelper::SaveStringToFile(CSV,*(Directory/TEXT("MovingTargetEncounters.csv")));
   for(const auto& F:M->Fighters()) if(F.Pawn.IsValid()) { F.Pawn->SetActorEnableCollision(true); F.Pawn->RespawnMatch(M->Base(F.Team)+FVector(F.Team?-2600:2600,0,0)); }
   M->SetActorTickEnabled(true); Delivery.Reset(); return true;
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultRangedEncounterTest,"CultUlhu.Menu.RangedEncounters",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultRangedEncounterTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FCultRangedEncounterObservation(this)); return true; }
#endif
