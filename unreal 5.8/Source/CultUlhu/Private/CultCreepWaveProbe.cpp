#if WITH_DEV_AUTOMATION_TESTS
#include "CultUlhuGameInstance.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMobaRoute.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "UObject/ObjectKey.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"

namespace {
struct FCreepWaveProbe {
 TWeakObjectPtr<UCultUlhuGameInstance> Game; int Phase=0; double Began=FPlatformTime::Seconds(); bool Shot=false;
 struct Sample { int Team,Lane,Kind; FVector Spawn,Last; float Travel=0,Progress=0; };
 TMap<FObjectKey,Sample> Units;
 bool Finish(bool TimedOut=false) {
  int Spawned[2][3]={},Moved[2][3]={},Siege[2][3]={}; float Progress[2][3]={}; TArray<FString> Rows; bool OK=!TimedOut;
  for(const auto& Pair:Units) { const auto& U=Pair.Value; ++Spawned[U.Team][U.Lane]; if(U.Travel>500) ++Moved[U.Team][U.Lane]; if(U.Kind==2&&U.Travel>500) ++Siege[U.Team][U.Lane]; Progress[U.Team][U.Lane]=FMath::Max(Progress[U.Team][U.Lane],U.Progress); }
  int DecorativeColliders=0;
  if(auto* G=Game.Get()) if(auto* W=G->GetWorld()) for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(TEXT("CultMobaRoad"))) {
   TArray<UStaticMeshComponent*> Meshes; It->GetComponents(Meshes); for(auto* C:Meshes) if(C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision) ++DecorativeColliders;
  }
  OK&=DecorativeColliders==0;
  for(int T=0;T<2;++T) for(int L=0;L<3;++L) {
   OK&=Spawned[T][L]>=13&&Moved[T][L]>=8&&Siege[T][L]>=1&&Progress[T][L]>.25f;
   Rows.Add(FString::Printf(TEXT("{\"team\":%d,\"lane\":%d,\"spawned\":%d,\"moved\":%d,\"moving_siege\":%d,\"max_progress\":%.4f}"),T,L,Spawned[T][L],Moved[T][L],Siege[T][L],Progress[T][L]));
   UE_LOG(LogTemp,Display,TEXT("[CultCreepWaveProbe] %s"),*Rows.Last());
  }
  const FString File=FPaths::ProjectSavedDir()/TEXT("Automation/CreepWavesStandalone.json"); IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
  FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"ok\":%s,\"timed_out\":%s,\"bots_active\":true,\"decorative_colliders\":%d,\"lanes\":[%s]}"),OK?TEXT("true"):TEXT("false"),TimedOut?TEXT("true"):TEXT("false"),DecorativeColliders,*FString::Join(Rows,TEXT(","))),*File);
  FPlatformMisc::RequestExitWithStatus(false,OK?0:1); return false;
 }
 void Diagnose(ACultMatchDirector* M) {
  auto* W=M->GetWorld(); bool Logged[2][3]={};
  for(TActorIterator<ACultMatchUnit> It(W);It;++It) if(!It->IsStructure()&&!Logged[It->Team][It->Lane]) {
   auto* U=*It; Logged[U->Team][U->Lane]=true; auto* C=U->FindComponentByClass<UCapsuleComponent>();
   FCollisionQueryParams Q(SCENE_QUERY_STAT(CreepDiagnostic),false,U); const auto Shape=FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()); const FCollisionObjectQueryParams Static(ECC_WorldStatic);
   const FVector P=U->GetActorLocation(),Raised=P+FVector(0,0,200),Goal=M->LaneWaypoint(U->Team,U->Lane,U->Waypoint),Next=Raised+(Goal-P).GetSafeNormal2D()*10;
   FHitResult Up,Across,Down; const bool A=W->SweepSingleByObjectType(Up,P,Raised,FQuat::Identity,Static,Shape,Q),B=W->SweepSingleByObjectType(Across,Raised,Next,FQuat::Identity,Static,Shape,Q),D=W->SweepSingleByObjectType(Down,Next,Next-FVector(0,0,500),FQuat::Identity,Static,Shape,Q);
   if(auto* Blocker=Up.GetActor()) {
    FString Tags; for(auto Tag:Blocker->Tags) Tags+=Tag.ToString()+TEXT(" ");
    UE_LOG(LogTemp,Display,TEXT("[CreepDiagnostic] blocker=%s tags=%s loaded=%d actorCollision=%d component=%s collision=%d"),*Blocker->GetPathName(),*Tags,int(Blocker->HasAnyFlags(RF_WasLoaded)),int(Blocker->GetActorEnableCollision()),*GetNameSafe(Up.GetComponent()),Up.GetComponent()?int(Up.GetComponent()->GetCollisionEnabled()):-1);
   }
   float HeroDistance=MAX_flt; FString Hero;
   for(const auto& F:M->Fighters()) if(F.Team!=U->Team&&F.Pawn.IsValid()) { FVector Closest; const float Dist=F.Pawn->FindComponentByClass<UCapsuleComponent>()->GetClosestPointOnCollision(P,Closest); if(Dist>=0&&Dist<HeroDistance) { HeroDistance=Dist; Hero=F.Name; } }
   UE_LOG(LogTemp,Display,TEXT("[CreepDiagnostic] team=%d lane=%d at=%s speed=%.0f stopped=%d tick=%d waypoint=%d range=%.0f hero=%s distance=%.0f up=%d/%s/penetration=%d across=%d/%s down=%d/%s normal=%s start=%d target=%s"),U->Team,U->Lane,*P.ToCompactString(),U->Speed,int(U->Stopped()),int(U->IsActorTickEnabled()),U->Waypoint,U->Range,*Hero,HeroDistance,int(A),*GetNameSafe(Up.GetActor()),int(Up.bStartPenetrating),int(B),*GetNameSafe(Across.GetActor()),int(D),*GetNameSafe(Down.GetActor()),*Down.ImpactNormal.ToCompactString(),int(Down.bStartPenetrating),*Goal.ToCompactString());
  }
 }
 bool Tick(float) {
  auto* G=Game.Get(); if(!G) return false; if(FPlatformTime::Seconds()-Began>150) return Finish(true);
  auto* W=G->GetWorld(); if(!W) return true;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Phase) { G->OpenCharacterSelect(ECultPlayMode::Dotgoo); G->SelectedCharacterId=TEXT("mi_go"); G->StartPlay(); Phase=1; } return true; }
  auto* M=ACultMatchDirector::Find(W); if(!M) return true;
  for(TActorIterator<ACultMatchUnit> It(W);It;++It) if(!It->IsStructure()&&!It->IsInTransit()) {
   auto* U=*It; const FVector P=U->GetActorLocation();
   auto* S=Units.Find(FObjectKey(U)); if(!S) S=&Units.Add(FObjectKey(U),{U->Team,U->Lane,U->Kind,P,P,0,0});
   S->Travel=FMath::Max(S->Travel,float(FVector::Dist2D(P,S->Spawn))); S->Progress=FMath::Max(S->Progress,FCultMobaRoute::Project(U->Team,U->Lane,P).Progress); S->Last=P;
  }
  if(M->Elapsed()>15&&!Shot) {
   auto* Camera=W->SpawnActor<ACameraActor>(FVector(-23000,-10000,16000),FRotator::ZeroRotator); Camera->SetActorRotation((FVector(-23000,0,0)-Camera->GetActorLocation()).Rotation());
   if(auto* PC=W->GetFirstPlayerController()) PC->SetViewTarget(Camera);
   FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/CreepWavesStandalone.png")),true,false); Shot=true;
  }
  if(M->Elapsed()>12&&FParse::Param(FCommandLine::Get(),TEXT("CultCreepWaveDiagnose"))) { Diagnose(M); return Finish(); }
  if(M->Elapsed()>=85||M->IsOver()) return Finish(); return true;
 }
};
}
void StartCultCreepWaveProbe(UCultUlhuGameInstance* Game) {
 if(!FParse::Param(FCommandLine::Get(),TEXT("CultCreepWaveProbe"))) return;
 auto Probe=MakeShared<FCreepWaveProbe>(); Probe->Game=Game;
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Dt){return Probe->Tick(Dt);}),.05f);
}
#endif
