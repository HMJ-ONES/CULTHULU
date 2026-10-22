#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultWardArchitecture.h"
#include "CultUlhuGameInstance.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultUlhuPlayerPawn.h"
#include "CultSkillAttack.h"
#include "CultCorpse.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "HighResScreenshot.h"
#include "StaticMeshResources.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);

class FCultWardArchitectureObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0,ShotIndex=0,Settle=0,Segment=0,Frames=0,OldEnabled=1; double Began=FPlatformTime::Seconds();
 FString Shot; TSharedFuture<FString> Written; TWeakObjectPtr<ACameraActor> Camera; TWeakObjectPtr<ACultMatchUnit> Ward,Nexus;
 FHighResScreenshotConfig OldCapture; uint32 OldX=0,OldY=0;
 struct FState { TWeakObjectPtr<ACultMatchUnit> Unit; FTransform Transform; float Health,Range,Dps,Radius,Half; ECollisionChannel Object; ECollisionResponse Pawn; };
 TArray<FState> States;
 void Apply(bool Refined) { for(auto& S:States) if(S.Unit.IsValid()) CultWardArchitecture::Compare(S.Unit.Get(),Refined); }
 void View(UWorld* W,int Index) {
  auto* PC=W->GetFirstPlayerController(); FVector Position,Focus;
  if(Index<2) { auto* U=Index?Nexus.Get():Ward.Get(); Focus=U->GetActorLocation()+FVector(0,0,-100); Position=U->GetActorLocation()+(Index?FVector(2200,-3600,2300):FVector(-1800,-3800,2300)); }
  else { Position=FVector(0,-39000,41000); Focus=FVector::ZeroVector; }
  Camera->SetActorLocation(Position); Camera->SetActorRotation((Focus-Position).Rotation()); PC->SetViewTarget(Camera.Get()); PC->PlayerCameraManager->UpdateCamera(0);
 }
 int VisibleMeshes() const {
  int Count=0; for(const auto& S:States) if(S.Unit.IsValid()) { TArray<UStaticMeshComponent*> Parts; S.Unit->GetComponents(Parts); for(auto* P:Parts) Count+=P->IsVisible()&&!P->bHiddenInGame; } return Count;
 }
 void Verify() {
  for(const auto& S:States) if(auto* U=S.Unit.Get()) {
   auto* Cap=U->FindComponentByClass<UCapsuleComponent>();
   Test->TestTrue(TEXT("Ward art preserves actor placement and scale"),S.Transform.Equals(U->GetActorTransform(),.01f));
   Test->TestTrue(TEXT("Ward art preserves combat stats and capsule dimensions"),U->Health==S.Health&&U->Range==S.Range&&U->Dps==S.Dps&&Cap->GetScaledCapsuleRadius()==S.Radius&&Cap->GetScaledCapsuleHalfHeight()==S.Half);
   Test->TestTrue(TEXT("Ward art preserves solid objective collision channels"),Cap->GetCollisionObjectType()==S.Object&&Cap->GetCollisionResponseToChannel(ECC_Pawn)==S.Pawn);
   TArray<UStaticMeshComponent*> Parts; U->GetComponents(Parts);
   for(auto* Part:Parts) if(Part->ComponentHasTag(TEXT("CultWardArchitecture"))) {
    Test->TestEqual(TEXT("New architecture supplies three shared material sections"),Part->GetNumMaterials(),3);
    Test->TestEqual(TEXT("Art never adds a second gameplay collision body"),Part->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    const auto Muzzle=Part->GetSocketLocation(TEXT("WardMuzzle"));
    Test->TestTrue(TEXT("Visible model muzzle socket matches the unchanged projectile origin"),Muzzle.Equals(U->GetActorLocation()+FVector(0,0,500),1));
    FBox Eye(ForceInit); const UStaticMesh* Mesh=Part->GetStaticMesh().Get(); const auto& LOD=Mesh->GetRenderData()->LODResources[0];
    for(const auto& Section:LOD.Sections) if(Mesh->GetMaterial(Section.MaterialIndex)->GetName()==TEXT("M_AttackSignal")) for(uint32 I=Section.FirstIndex;I<Section.FirstIndex+Section.NumTriangles*3;++I) {
     const FVector V(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I))); if(V.Z>0) Eye+=V;
    }
    Test->TestTrue(TEXT("Actual glowing emitter vertices surround the projectile muzzle"),Eye.IsValid&&Eye.GetCenter().Equals(FVector(0,0,500),1));
   }
  }
 }
 void Corpse(UWorld* W) {
  auto* Variable=IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Match.WardArt")); Variable->Set(1,ECVF_SetByCode);
  const FTransform T(FVector(0,18000,1253)); auto* U=W->SpawnActorDeferred<ACultMatchUnit>(ACultMatchUnit::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  U->Kind=3; U->Team=0; U->Health=3000; U->Range=U->Dps=0; U->FinishSpawning(T);
  TArray<UStaticMeshComponent*> Parts; U->GetComponents(Parts); Test->TestEqual(TEXT("Production ward creates one mesh component, without legacy components"),Parts.Num(),1);
  if(Parts.Num()!=1) { U->Destroy(); return; }
  UStaticMesh* Asset=Parts[0]->GetStaticMesh().Get(); TArray<ACultCorpse*> Before; for(TActorIterator<ACultCorpse> It(W);It;++It) Before.Add(*It);
  UMaterialInstanceDynamic* LiveGlow=nullptr; TArray<UStaticMeshComponent*> LiveParts; Ward->GetComponents(LiveParts);
  for(auto* P:LiveParts) if(P->ComponentHasTag(TEXT("CultWardArchitecture"))) for(int I=0;I<P->GetNumMaterials();++I) if(P->GetStaticMesh()->GetMaterial(I)->GetName()==TEXT("M_AttackSignal")) LiveGlow=Cast<UMaterialInstanceDynamic>(P->GetMaterial(I));
  const float LiveIntensity=LiveGlow?LiveGlow->K2_GetScalarParameterValue(TEXT("Intensity")):0;
  U->Die(); ACultCorpse* Remains=nullptr; for(TActorIterator<ACultCorpse> It(W);It;++It) if(!Before.Contains(*It)) Remains=*It;
  Test->TestNotNull(TEXT("The assembled ward leaves persistent architecture remains"),Remains);
  if(Remains) { TArray<UStaticMeshComponent*> Copies; Remains->GetComponents(Copies);
   Test->TestTrue(TEXT("Remains retain the exact assembled mesh"),Copies.Num()==1&&Copies[0]->GetStaticMesh()==Asset);
   if(Copies.Num()==1) Test->TestEqual(TEXT("Architecture remains do not block movement"),Copies[0]->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
   if(Copies.Num()==1) for(int I=0;I<Copies[0]->GetNumMaterials();++I) if(Asset->GetMaterial(I)->GetName()==TEXT("M_AttackSignal")) {
    auto* Glow=Cast<UMaterialInstanceDynamic>(Copies[0]->GetMaterial(I)); Test->TestNotNull(TEXT("Ward remains retain their own signal material"),Glow);
    if(Glow) Test->TestEqual(TEXT("A destroyed ward has no live emitter glow"),Glow->K2_GetScalarParameterValue(TEXT("Intensity")),0.f);
   }
   Test->TestTrue(TEXT("Extinguishing one ward cannot darken another live ward"),LiveGlow&&LiveIntensity>0&&LiveGlow->K2_GetScalarParameterValue(TEXT("Intensity"))==LiveIntensity);
   Test->TestTrue(TEXT("Rotated ward remains rest on the ground"),FMath::Abs(Remains->GetComponentsBoundingBox(true).Min.Z-3)<5);
  }
 }
 void ProjectileOrigins(UWorld* W,ACultMatchDirector* Match) {
  for(auto* U:{Ward.Get(),Nexus.Get()}) {
   const float Cooldown=U->AttackCooldown; U->AttackCooldown=0;
   U->FireAt(Match->Fighters()[U->Team?0:5].Pawn.Get()); int Count=0;
   UStaticMeshComponent* Body=nullptr; TArray<UStaticMeshComponent*> Parts; U->GetComponents(Parts); for(auto* P:Parts) if(P->ComponentHasTag(TEXT("CultWardArchitecture"))) Body=P;
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==U) {
    ++Count; Test->TestTrue(TEXT("Real fired projectile starts at the assembled model's emitter"),Body&&It->GetActorLocation().Equals(Body->GetSocketLocation(TEXT("WardMuzzle")),1)); It->Destroy();
   }
   Test->TestEqual(TEXT("A native objective firing event creates one real projectile"),Count,1); U->AttackCooldown=Cooldown;
  }
 }
public:
 explicit FCultWardArchitectureObservation(FAutomationTestBase* In):Test(In) {
  OldCapture=GetHighResScreenshotConfig(); OldX=GScreenshotResolutionX; OldY=GScreenshotResolutionY;
  auto* Variable=IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Match.WardArt")); OldEnabled=Variable->GetInt(); Variable->Set(0,ECVF_SetByCode);
 }
 ~FCultWardArchitectureObservation() {
  GetHighResScreenshotConfig()=OldCapture; GScreenshotResolutionX=OldX; GScreenshotResolutionY=OldY;
  IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Match.WardArt"))->Set(OldEnabled,ECVF_SetByCode);
  if(auto* W=GEditor->PlayWorld.Get()) UGameplayStatics::SetGamePaused(W,false);
 }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>180) { Test->AddError(TEXT("Ward architecture render check timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(ECultPlayMode::Dotgoo); G->OpenBeliefSelect(); Phase=1; } return false; }
  auto* M=ACultMatchDirector::Find(W); if(!M||W->GetTimeSeconds()<1) return false;
  if(Phase==1) {
   UGameplayStatics::SetGamePaused(W,true);
   for(TActorIterator<ACultMatchUnit> It(W);It;++It) if(It->Kind==3||It->Kind==5) {
    auto* U=*It; auto* C=U->FindComponentByClass<UCapsuleComponent>(); States.Add({U,U->GetActorTransform(),U->Health,U->Range,U->Dps,C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight(),C->GetCollisionObjectType(),C->GetCollisionResponseToChannel(ECC_Pawn)});
    if(U->Kind==3&&U->Team==1&&U->Lane==1&&!Ward.IsValid()) Ward=U; if(U->Kind==5&&U->Team==0) Nexus=U;
   }
   Test->TestEqual(TEXT("Fourteen real lane and nexus objectives participate"),States.Num(),14);
   if(!Ward.IsValid()||!Nexus.IsValid()) { Test->AddError(TEXT("Missing real objective subjects")); return true; }
   Camera=W->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->bConstrainAspectRatio=false;
   Apply(false); Test->TestEqual(TEXT("Legacy comparison uses seven visible parts per ward"),VisibleMeshes(),98); View(W,0); Settle=30; Phase=2; return false;
  }
  if(Settle>0) { --Settle; return false; }
  if(Phase==2) {
   const TCHAR* Names[]={TEXT("LegacyWard"),TEXT("LegacyNexus"),TEXT("LegacyOverview"),TEXT("StoneWard"),TEXT("StoneNexus"),TEXT("StoneOverview")};
   Shot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/WardArchitecture_%s.png"),Names[ShotIndex])); IFileManager::Get().Delete(*Shot);
   auto& Capture=GetHighResScreenshotConfig(); Capture.SetHDRCapture(false); Capture.SetMaskEnabled(false); Capture.SetForce128BitRendering(false); Capture.SetResolution(1280,720); Capture.SetFilename(Shot);
   if(!W->GetGameViewport()->GetGameViewport()->TakeHighResScreenShot()) { Test->AddError(TEXT("Ward capture failed")); return true; } Phase=3; return false;
  }
  if(Phase==3) {
   if(FScreenshotRequest::IsScreenshotRequested()) return false; TArray<uint8> Bytes; if(!FFileHelper::LoadFileToArray(Bytes,*Shot)||Bytes.Num()<24) return false;
   auto Size=[&](int N){return (uint32(Bytes[N])<<24)|(uint32(Bytes[N+1])<<16)|(uint32(Bytes[N+2])<<8)|uint32(Bytes[N+3]);};
   Test->TestTrue(TEXT("Ward review image is an exact 1280x720 render"),Size(16)==1280&&Size(20)==720); Verify();
   if(++ShotIndex<6) { if(ShotIndex==3) { Apply(true); Test->TestEqual(TEXT("Refined presentation uses one visible mesh per ward"),VisibleMeshes(),14); } View(W,ShotIndex%3); Settle=30; Phase=2; return false; }
   Apply(false); View(W,2); Settle=45; Phase=4; return false;
  }
  if(Phase==4) {
   const auto Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Reports/Captures/WardArchitecture")); IFileManager::Get().MakeDirectory(*Dir,true);
   FCsvProfiler::Get()->BeginCapture(-1,Dir,FString::Printf(TEXT("Ward_%d.csv"),Segment)); Frames=0; Phase=5; return false;
  }
  if(Phase==5) {
   CSV_CUSTOM_STAT(CultRuntime,WardArtEnabled,Segment==1||Segment==2,ECsvCustomStatOp::Set);
   CSV_CUSTOM_STAT(CultRuntime,WardVisibleMeshes,VisibleMeshes(),ECsvCustomStatOp::Set);
   if(++Frames<120) return false; Written=FCsvProfiler::Get()->EndCapture(); Phase=6; return false;
  }
  if(!Written.IsReady()) return false;
  Test->TestTrue(TEXT("Ward frozen-view render-cost CSV exists"),IFileManager::Get().FileSize(*Written.Get())>0); Test->AddInfo(TEXT("Ward profile ")+Written.Get()); Verify();
  if(++Segment<4) { Apply(Segment==1||Segment==2); Settle=45; Phase=4; return false; }
  Apply(true); ProjectileOrigins(W,M); Corpse(W); Test->AddInfo(TEXT("Ward architecture: fourteen subjects, six exact captures, preserved combat/collision, emitter geometry and real projectile alignment, one-mesh production corpse, four frozen A-B-B-A windows.")); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultWardArchitectureTest,"CultUlhu.Menu.WardArchitecture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultWardArchitectureTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultWardArchitectureObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
