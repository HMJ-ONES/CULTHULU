#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultJungleCliffs.h"
#include "CultMobaJungle.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);

class FCultJungleCliffObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; double Began=FPlatformTime::Seconds(); int Phase=0,Frame=0,Settle=0,Segment=0,Frames=0;
 TSharedFuture<FString> Written;
 TWeakObjectPtr<APlayerController> Controller; TWeakObjectPtr<AActor> Original; TWeakObjectPtr<ACameraActor> Camera;
 FHighResScreenshotConfig Previous; uint32 PreviousX=0,PreviousY=0; FString Shot; FTransform HeroTransform;
 TArray<UProceduralMeshComponent*> Cliffs;
 void Geometry(UWorld* W,ACultUlhuPlayerPawn* P) {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(JungleCliffSurface),true); int Triangles=0,Matched=0,Supports=0;
  for(auto* Mesh:Cliffs) {
   for(int Section=0;Section<Mesh->GetNumSections();++Section) {
    auto* S=Mesh->GetProcMeshSection(Section); Test->TestTrue(TEXT("Every visible cliff band has collision enabled"),S&&S->bEnableCollision); if(!S) continue;
    for(int I=0;I<S->ProcIndexBuffer.Num();I+=3) {
     FVector V[3]; for(int J=0;J<3;++J) V[J]=Mesh->GetComponentTransform().TransformPosition(S->ProcVertexBuffer[S->ProcIndexBuffer[I+J]].Position);
     const auto Point=(V[0]+V[1]+V[2])/3,Normal=-FVector::CrossProduct(V[1]-V[0],V[2]-V[0]).GetSafeNormal(); FHitResult H;
     const bool Hit=Mesh->LineTraceComponent(H,Point+Normal*40,Point-Normal*40,Q);
     ++Triangles; if(Hit&&FVector::Dist(H.ImpactPoint,Point)<2) ++Matched;
     else if(Triangles-Matched<=4) Test->AddInfo(FString::Printf(TEXT("Cliff triangle mismatch section%d tri%d centre%s normal%s hit%d at%s"),Section,I/3,*Point.ToCompactString(),*Normal.ToCompactString(),Hit,*H.ImpactPoint.ToCompactString()));
    }
   }
   const auto Base=Mesh->GetComponentLocation(); const float Height=Mesh->GetProcMeshSection(0)->ProcVertexBuffer[0].Position.Z;
   FHitResult Wall; Test->TestTrue(TEXT("A cliff face blocks a direct ground-level approach"),Mesh->LineTraceComponent(Wall,Base+FVector(6000,0,Height*.5f),Base+FVector(0,0,Height*.5f),Q)&&Wall.ImpactNormal.Z<.65f);
   const auto* Cap=P->GetCapsuleComponent(); const auto Shape=FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius(),Cap->GetScaledCapsuleHalfHeight());
   for(auto Offset:{FVector(0,0,0),FVector(1500,0,0),FVector(-1500,0,0),FVector(0,2000,0),FVector(0,-2000,0)}) {
    const auto Root=Base+Offset+FVector(0,0,Height+Cap->GetScaledCapsuleHalfHeight()); FHitResult H;
    if(W->SweepSingleByObjectType(H,Root+FVector(0,0,100),Root-FVector(0,0,100),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),Shape,Q)&&H.GetComponent()==Mesh&&H.ImpactNormal.Z>.9f&&FMath::Abs(H.ImpactPoint.Z-(Base.Z+Height))<2) ++Supports;
   }
  }
  Test->TestEqual(TEXT("Every rendered triangle has matching physical collision"),Matched,Triangles);
  Test->TestEqual(TEXT("All four plateaus support the full-height vessel at five landing samples"),Supports,20);
  int Mirror=0;
  for(int Pair=0;Pair<2;++Pair) for(int I=0;I<12;++I) {
   const auto A=FCultMobaJungle::Ground(Pair*2),B=FCultMobaJungle::Ground(Pair*2+1); const float Angle=I*2*PI/12;
   auto* Left=Cliffs.FindByPredicate([&](auto* C){return FMath::Abs(C->GetComponentLocation().X-A.X)<1&&FMath::Abs(C->GetComponentLocation().Y-A.Y)<1;});
   auto* Right=Cliffs.FindByPredicate([&](auto* C){return FMath::Abs(C->GetComponentLocation().X-B.X)<1&&FMath::Abs(C->GetComponentLocation().Y-B.Y)<1;});
   if(!Left||!Right) continue; FHitResult HA,HB;
   const FVector O(FMath::Cos(Angle)*6000,FMath::Sin(Angle)*4500,-A.Z*.5f);
   if((*Left)->LineTraceComponent(HA,A+O,A-FVector(0,0,A.Z*.5f),Q)&&(*Right)->LineTraceComponent(HB,B+FVector(-O.X,O.Y,O.Z),B-FVector(0,0,B.Z*.5f),Q)) {
    const auto DA=HA.ImpactPoint-A,DB=HB.ImpactPoint-B; if(FMath::Abs(DA.X+DB.X)<2&&FMath::Abs(DA.Y-DB.Y)<2&&FMath::Abs(DA.Z-DB.Z)<2) ++Mirror;
   }
  }
  Test->TestEqual(TEXT("Both teams have mirrored physical cliff contours"),Mirror,24);
  Test->AddInfo(FString::Printf(TEXT("Jungle cliffs: %d/%d visual triangles match collision; %d/20 full-height supports; %d/24 mirrored contour rays"),Matched,Triangles,Supports,Mirror));
 }
public:
 explicit FCultJungleCliffObservation(FAutomationTestBase* T):Test(T) { Previous=GetHighResScreenshotConfig(); PreviousX=GScreenshotResolutionX; PreviousY=GScreenshotResolutionY; }
 ~FCultJungleCliffObservation() { GetHighResScreenshotConfig()=Previous; GScreenshotResolutionX=PreviousX; GScreenshotResolutionY=PreviousY; if(auto* PC=Controller.Get()) { PC->SetPause(false); if(Original.IsValid()) PC->SetViewTarget(Original.Get()); } if(Camera.IsValid()) Camera->Destroy(); }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>180) { Test->AddError(TEXT("Jungle cliff validation timed out")); if(FCsvProfiler::Get()->IsCapturing()) FCsvProfiler::Get()->EndCapture(); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Phase) { auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->SelectedCharacterId=TEXT("cthulhu_avatar"); GI->OpenCharacterSelect(ECultPlayMode::Dotgoo); GI->OpenBeliefSelect(); Phase=1; } return false; }
  auto* PC=W->GetFirstPlayerController(); auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0)); if(!PC||!P||W->GetTimeSeconds()<1) return false;
  if(Phase==1) {
   Controller=PC; Original=PC->GetViewTarget(); HeroTransform=P->GetActorTransform(); PC->SetPause(true);
   for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(TEXT("CultMobaJungleMesa"))) if(auto* Mesh=It->FindComponentByClass<UProceduralMeshComponent>()) Cliffs.Add(Mesh);
   Test->TestEqual(TEXT("Four generated collision cliffs replace the cylindrical mesas"),Cliffs.Num(),4); if(Cliffs.Num()!=4) return true;
   Geometry(W,P); Test->TestEqual(TEXT("Legacy comparison affects only the four plateau representations"),CultJungleCliffs::LegacyComparison(W,true),4);
   Camera=W->SpawnActor<ACameraActor>(); PC->SetViewTarget(Camera.Get()); Phase=2; Settle=20;
  }
  if(Settle>0) { --Settle; return false; }
  if(Phase==2) {
   if(Frame==2) CultJungleCliffs::LegacyComparison(W,false);
   const FVector Position=Frame%2?FCultMobaJungle::Ground(2)+FVector(-10000,0,3500):FVector(-14000,-14000,9000);
   const FVector Focus=Frame%2?FCultMobaJungle::Ground(2)+FVector(0,0,900):FVector(0,0,1800);
   Camera->SetActorLocation(Position); Camera->SetActorRotation((Focus-Position).Rotation()); PC->PlayerCameraManager->UpdateCamera(0);
   Settle=20; Phase=3; return false;
  }
  if(Phase==3) {
   const TCHAR* Names[]={TEXT("CylinderOverview"),TEXT("CylinderNear"),TEXT("CliffsOverview"),TEXT("CliffsNear")};
   Shot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/JungleGeology_%s.png"),Names[Frame])); IFileManager::Get().Delete(*Shot);
   auto& Capture=GetHighResScreenshotConfig(); Capture.SetHDRCapture(false); Capture.SetMaskEnabled(false); Capture.SetForce128BitRendering(false); Capture.SetResolution(1280,720); Capture.SetFilename(Shot);
   if(!W->GetGameViewport()->GetGameViewport()->TakeHighResScreenShot()) { Test->AddError(TEXT("Jungle comparison capture failed")); return true; } Phase=4; return false;
  }
  if(Phase==5) {
   const auto Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Reports/Captures/JungleCliffs")); IFileManager::Get().MakeDirectory(*Dir,true);
   FCsvProfiler::Get()->BeginCapture(-1,Dir,FString::Printf(TEXT("JungleCliffs_%d.csv"),Segment)); Frames=0; Phase=6; return false;
  }
  if(Phase==6) { CSV_CUSTOM_STAT(CultRuntime,JungleCliffStyle,Segment==1||Segment==2,ECsvCustomStatOp::Set); if(++Frames<120) return false; Written=FCsvProfiler::Get()->EndCapture(); Phase=7; return false; }
  if(Phase==7) {
   if(!Written.IsReady()) return false; Test->TestTrue(TEXT("Cliff render-cost profile was written"),IFileManager::Get().FileSize(*Written.Get())>0); Test->AddInfo(TEXT("Jungle cliff profile: ")+Written.Get());
   if(++Segment<4) { CultJungleCliffs::LegacyComparison(W,!(Segment==1||Segment==2)); Settle=30; Phase=5; return false; }
   CultJungleCliffs::LegacyComparison(W,false); return true;
  }
  if(FScreenshotRequest::IsScreenshotRequested()) return false;
  TArray<uint8> Data; if(!FFileHelper::LoadFileToArray(Data,*Shot)||Data.Num()<24) return false;
  auto Size=[&](int At){return (uint32(Data[At])<<24)|(uint32(Data[At+1])<<16)|(uint32(Data[At+2])<<8)|Data[At+3];};
  Test->TestTrue(TEXT("Geology comparison is an exact 1280x720 render"),Size(16)==1280&&Size(20)==720);
  Test->TestTrue(TEXT("Cliff comparison preserves entity size and physical location"),HeroTransform.Equals(P->GetActorTransform(),.01f));
  for(auto* Mesh:Cliffs) {
   auto* Legacy=Mesh->GetOwner()->FindComponentByClass<UStaticMeshComponent>();
   Test->TestTrue(TEXT("Only the visible representation supplies collision"),Legacy&&((Frame<2&&Legacy->IsVisible()&&Legacy->GetCollisionEnabled()!=ECollisionEnabled::NoCollision&&!Mesh->IsVisible()&&Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision)||(Frame>=2&&!Legacy->IsVisible()&&Legacy->GetCollisionEnabled()==ECollisionEnabled::NoCollision&&Mesh->IsVisible()&&Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)));
  }
  if(++Frame<4) { Phase=2; return false; }
  const FVector Site(-14000,-14000,9000); Camera->SetActorLocation(Site); Camera->SetActorRotation((FVector(0,0,1800)-Site).Rotation()); PC->PlayerCameraManager->UpdateCamera(0);
  CultJungleCliffs::LegacyComparison(W,true); Settle=30; Phase=5; return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultJungleCliffTest,"CultUlhu.Menu.JungleCliffGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultJungleCliffTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultJungleCliffObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
