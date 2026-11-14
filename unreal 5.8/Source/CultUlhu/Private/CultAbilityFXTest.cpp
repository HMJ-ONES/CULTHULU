#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CultAbilityFX.h"
#include "CultAutomationPIE.h"
#include "Camera/CameraActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "ProceduralMeshComponent.h"
#include "RenderingThread.h"
#include "Tests/AutomationEditorCommon.h"

namespace {
class FAbilityFilamentCapture final : public IAutomationLatentCommand {
 FAutomationTestBase* Test;
 TWeakObjectPtr<ASceneCapture2D> Camera;
 TStrongObjectPtr<UTextureRenderTarget2D> Target;
 int32 Frames=0;
 double Started=FPlatformTime::Seconds();

public:
 explicit FAbilityFilamentCapture(FAutomationTestBase* InTest):Test(InTest) {}

 bool Update() override {
  using namespace CultAbilityFX;
  if(FPlatformTime::Seconds()-Started>75) { Test->AddError(TEXT("Ability visual capture timed out")); return true; }
  UWorld* World=GEditor->PlayWorld.Get();
  if(!World||World->bIsTearingDown||!World->GetFirstPlayerController()) return false;
  if(!Camera.IsValid()) {
   const FVector Origin(0,0,44000);
   const FVector Eye=Origin+FVector(0,-3100,2030);
   const FRotator View=(Origin+FVector(0,0,180)-Eye).Rotation();
   auto* Observer=World->SpawnActor<ACameraActor>(Eye,View);
   auto* Player=World->GetFirstPlayerController();
   Player->SetViewTarget(Observer);
   Player->PlayerCameraManager->UpdateCamera(0);

   Camera=World->SpawnActor<ASceneCapture2D>(Eye,View);
   auto* Capture=Camera->GetCaptureComponent2D();
   Target.Reset(NewObject<UTextureRenderTarget2D>(Camera.Get()));
   Target->ClearColor=FLinearColor::Black;
   Target->RenderTargetFormat=RTF_RGBA8;
   Target->InitAutoFormat(1280,560);
   Target->UpdateResourceImmediate(true);
   Capture->TextureTarget=Target.Get();
   Capture->CaptureSource=SCS_FinalColorLDR;
   Capture->ProjectionType=ECameraProjectionMode::Perspective;
   Capture->FOVAngle=61;
   Capture->bCaptureEveryFrame=false;
   Capture->bCaptureOnMovement=false;
   Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
   Capture->ShowFlags.SetFog(false);
   Capture->ShowFlags.SetAtmosphere(false);
   Capture->ShowFlags.SetMotionBlur(false);
   Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;
   Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
   Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
   Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
   Capture->PostProcessSettings.bOverride_AutoExposureBias=true;
   Capture->PostProcessSettings.AutoExposureBias=1;

   const ELook Looks[]={ELook::Tendrils,ELook::Ember,ELook::Gate,ELook::Mend};
   for(int32 Index=0;Index<UE_ARRAY_COUNT(Looks);++Index) {
    const FVector Point=Origin+FVector((Index-1.5)*1000,0,0);
    auto* FX=ACultAbilityFX::Spawn(World,Looks[Index],EEvent::Release,Point,FVector::ForwardVector,620,nullptr,0,.15f);
    Test->TestNotNull(TEXT("Ability burst creates a real local effect"),FX);
    if(!FX) continue;
    FX->SetActorTickEnabled(false);
    FX->SetLifeSpan(0);
    Capture->ShowOnlyActorComponents(FX);
    auto* Mesh=FX->FindComponentByClass<UProceduralMeshComponent>();
    Test->TestNotNull(TEXT("Ability effect owns one procedural mesh"),Mesh);
    if(!Mesh) continue;
    Test->TestEqual(TEXT("The additive shape is cosmetic"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    Test->TestTrue(TEXT("The baked material is soft additive"),Mesh->GetMaterial(0)&&Mesh->GetMaterial(0)->GetBlendMode()==BLEND_Additive);
    const auto* Section=Mesh->GetProcMeshSection(0);
    Test->TestTrue(TEXT("The filament budget stays bounded"),Section&&Section->ProcVertexBuffer.Num()>40&&Section->ProcVertexBuffer.Num()<1000);
   }
   return false;
  }
  if(++Frames<12) return false;
  if(Frames<=15) { Camera->GetCaptureComponent2D()->CaptureScene(); return false; }
  FlushRenderingCommands();
  TArray<FColor> Pixels;
  Test->TestTrue(TEXT("Read rendered ability effects"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
  if(Pixels.Num()!=1280*560) return true;
  int32 LitPixels[4]={};
  for(int32 Index=0;Index<Pixels.Num();++Index) {
   const auto& P=Pixels[Index];
   if(FMath::Max3(P.R,P.G,P.B)>60) ++LitPixels[(Index%1280)/320];
  }
  for(int32 Index=0;Index<4;++Index) {
   Test->TestTrue(FString::Printf(TEXT("Effect panel %d visibly renders (%d lit pixels)"),Index,LitPixels[Index]),LitPixels[Index]>65);
  }
  TArray64<uint8> PNG;
  FImageUtils::PNGCompressImageArray(1280,560,MakeArrayView(Pixels),PNG);
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Screenshots");
  IFileManager::Get().MakeDirectory(*Directory,true);
  Test->TestTrue(TEXT("Save the rendered ability effect comparison"),FFileHelper::SaveArrayToFile(PNG,*(Directory/TEXT("AbilityFilaments.png"))));
  return true;
 }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultAbilityFXTest,"CultUlhu.Art.AbilityFilaments",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultAbilityFXTest::RunTest(const FString&) {
 using namespace CultAbilityFX;
 static const TCHAR* Roster[]={
#define CULT_RELEASE_CHARACTER(Id) TEXT(Id),
#include "CultReleaseRoster.inl"
#undef CULT_RELEASE_CHARACTER
 };
 for(const TCHAR* Id:Roster) for(int32 Slot=-1;Slot<=2;++Slot) {
  TestTrue(FString::Printf(TEXT("%s ability %d has a visual identity"),Id,Slot),ForSlot(Id,Slot)!=ELook::None);
 }
 ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE());
 ADD_LATENT_AUTOMATION_COMMAND(FAbilityFilamentCapture(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
